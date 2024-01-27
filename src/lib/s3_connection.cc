#include "lib/s3_connection.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/s3_error.h"
#include "lib/s3_memory.h"
#include "lib/s3_socket.h"

#define CONNECTION_BUF_SIZE                16*1024*1024
#define CONNECTION_BUF_MIN_UNCONSUMED_SIZE 1*1024*1024
#define CONNECTION_BUF_MIN_LEFT_SIZE       1*1024*1024

S3Connection *s3_connection_construct() {
    S3Connection *c = (S3Connection*)s3_malloc(sizeof(S3Connection));
    if (c != NULL) {
        *c = (S3Connection)s3_connection_null;
    }
    return c;
}

void s3_connection_destruct(S3Connection *c) {
    if (c != NULL) {
        s3_connection_destroy(c);
        s3_free(c, sizeof(S3Connection));
    }
}

int s3_connection_init(S3Connection *c, int epfd, int fd) {
    s3_socket_set_non_blocking(fd);
    c->epfd          = epfd;
    c->fd            = fd;

    S3Buf *buf = s3_buf_construct();
    assert(buf != NULL);
    s3_buf_init(buf, CONNECTION_BUF_SIZE);
    c->recv_buf = buf;

    buf = s3_buf_construct();
    assert(buf != NULL);
    s3_buf_init(buf, CONNECTION_BUF_SIZE);
    c->send_buf = buf;

    s3_object_req_init(&c->object_req);

    c->closed = CONN_CLOSE_STATUS_NONE;
    c->status_code = S3_OBJECT_STATUS_CODE_SUCC;

    return S3_OK;
}

void s3_connection_destroy(S3Connection *c) {
    close(c->fd);
    s3_object_req_destroy(&c->object_req);

    s3_buf_destruct(c->recv_buf);
    s3_buf_destruct(c->send_buf);
}

void s3_connection_try_destruct(S3Connection *c) {
#ifdef DEBUG
    printf("try destruct connection. unconsumed size=%ld, fd=%d, c->closed=%d\n",
            s3_buf_unconsumed_size(c->send_buf), c->fd, c->closed);
#endif
    /*
     * 如果有待发送的数据以及连接正常，需等待
     */
    if (s3_buf_unconsumed_size(c->send_buf) != 0 &&
        c->closed != CONN_CLOSE_STATUS_COMPLETE) {
        return;
    }

    int ret = 0;
    ret = epoll_ctl(c->epfd, EPOLL_CTL_DEL, c->fd, &c->ev);
    assert(ret == 0);

    s3_connection_destruct(c);
}

static int s3_connection_process_recv_msg(S3Connection *c) {
    if (c->object_req.req_code == S3_OBJECT_CODE_START) {
        if (s3_buf_unconsumed_size(c->recv_buf) < S3_OBJECT_REQ_HEADER_SIZE) {
            return S3_OK;
        }
        s3_object_req_header_decode(&c->object_req, c->recv_buf);
        assert(c->object_req.req_code != S3_OBJECT_CODE_START);
    }
    int ret = (c->handler->process)(c);
    assert(ret == S3_OK);
    return ret;
}

static void s3_connection_reset_buf(S3Buf *b) {
    if (s3_buf_unconsumed_size(b) == 0) {
        b->left = b->right = b->data;
        return;
    }

    int unconsumed = s3_buf_unconsumed_size(b);
    if (unconsumed < CONNECTION_BUF_MIN_UNCONSUMED_SIZE
        && s3_buf_free_size(b) < CONNECTION_BUF_MIN_LEFT_SIZE) {
        memcpy(b->data, b->left, unconsumed);
        b->left = b->data;
        b->right = b->data + unconsumed;
        return;
    }
}

void s3_connection_recv_socket_cb(S3Connection *c) {
    assert(c != NULL);

    int n = s3_socket_read(c->fd, c->recv_buf->right, s3_buf_free_size(c->recv_buf));
    if (n < 0) {
        if (n == S3_ERR_NET_AGAIN) {
            return;
        }

        // S3_ERR_NET_ABORT, net error, force close connection
        c->closed = CONN_CLOSE_STATUS_COMPLETE;
        (c->handler->close)(c);
        return;
    } else if (n == 0) {
#ifdef DEBUG
        printf("peer connection close. fd=%d\n", c->fd);
#endif
        (c->handler->close)(c);
        return;
    }
    c->recv_buf->right += n;

    int ret = s3_connection_process_recv_msg(c);
    assert(ret == S3_OK);

    s3_connection_reset_buf(c->recv_buf);
}

void s3_connection_send_socket_cb(S3Connection *c) {
    int ret = S3_OK;
    ret = s3_connection_send_msg_buf(c);
    if (ret == S3_ERR_NET_ABORT) { // connection has closed
        return;
    }

    ret = (c->handler->process)(c);
    assert(ret == S3_OK);
}

int s3_connection_send_msg_buf(S3Connection *c) {
    assert(c != NULL);
    int n = s3_socket_write(c->fd, c->send_buf->left, s3_buf_unconsumed_size(c->send_buf));
    if (n < 0) {
        if (n == S3_ERR_NET_AGAIN) {
            return S3_ERR_NET_AGAIN;
        }

        // S3_ERR_NET_ABORT, net error, force close connection
        c->closed = CONN_CLOSE_STATUS_COMPLETE;
        (c->handler->close)(c);
        return S3_ERR_NET_ABORT;
    }
    c->send_buf->left += n;

    s3_connection_reset_buf(c->send_buf);
    return S3_OK;
}

void s3_connection_do_resp(S3Connection *c) {
    if (c->object_req.resped) {
        return;
    }

    S3ObjectRespHeader header;
    header.resp_code = c->object_req.req_code + 1;
    header.status_code = c->status_code;
    header.file_len = c->object_req.file_len;
    //memcpy(header.file_name, c->object_req.file_name, c->object_req.file_name_len);
    //header.file_name[c->object_req.file_name_len] = '\0';
    strcpy(header.file_name, c->object_req.file_name);

    memcpy(c->send_buf->right, &header, S3_OBJECT_RESP_HEADER_SIZE);
    c->send_buf->right += S3_OBJECT_RESP_HEADER_SIZE;
    c->object_req.resped = true;

    s3_connection_open_epollout(c);
    s3_connection_send_msg_buf(c);
}

void s3_connection_do_half_close(S3Connection* c) {
    if (c->object_req.step == S3_OBJECT_REQ_STEP_BODY) {
        c->object_req.step = S3_OBJECT_REQ_STEP_END;
        int ret = (c->handler->process)(c);
        assert(ret == S3_OK);
    }
}

void s3_connection_open_epollout(S3Connection *c) {
    c->ev.events |= EPOLLOUT;
    int ret = epoll_ctl(c->epfd, EPOLL_CTL_MOD, c->fd, &c->ev);
    assert(ret == 0);
}

void s3_connection_close_epollout(S3Connection *c) {
    uint32_t epoll_out = EPOLLOUT;
    c->ev.events ^= epoll_out;
    int ret = epoll_ctl(c->epfd, EPOLL_CTL_MOD, c->fd, &c->ev);
    assert(ret == 0);
}
