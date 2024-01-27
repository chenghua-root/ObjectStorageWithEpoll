#ifndef S3_LIB_CONNECTION_H_
#define S3_LIB_CONNECTION_H_

#include <list>
#include <stdint.h>
#include <sys/epoll.h>

#include "lib/s3_buf.h"
#include "lib/s3_io_handler.h"
#include "lib/s3_object_req.h"

enum CONN_CLOSE_STATUS {
    CONN_CLOSE_STATUS_NONE     = 0,  // 连接正常
    CONN_CLOSE_STATUS_HALF     = 1,  // 连接半关闭，可发送数据，如客户端shutdown
    CONN_CLOSE_STATUS_COMPLETE = 2,  // 连接全关闭，如网络出错，客户端close
};

struct S3Connection {
    S3IOHandler        *handler;
    int                epfd;
    struct epoll_event ev;

    int                fd;
    void               *ioth;

    S3ObjectReq        object_req;

    S3Buf              *recv_buf;
    S3Buf              *send_buf;
    CONN_CLOSE_STATUS  closed;

    S3_OBJECT_STATUS_CODE  status_code;
};
#define s3_connection_null { \
    .fd = -1,                \
    .ioth = NULL,            \
    .recv_buf = NULL,        \
}

S3Connection *s3_connection_construct();
void s3_connection_destruct(S3Connection *c);
int s3_connection_init(S3Connection *c, int epfd, int fd);
void s3_connection_destroy(S3Connection *c);

void s3_connection_try_destruct(S3Connection *c);

void s3_connection_recv_socket_cb(S3Connection *c);
void s3_connection_send_socket_cb(S3Connection *c);
int s3_connection_send_msg_buf(S3Connection *c);

void s3_connection_do_resp(S3Connection *c);
void s3_connection_do_half_close(S3Connection* c);

void s3_connection_open_epollout(S3Connection *c);
void s3_connection_close_epollout(S3Connection *c);

#endif
