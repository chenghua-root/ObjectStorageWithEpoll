#include "s3_io.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "lib/s3_error.h"
#include "lib/s3_memory.h"
#include "lib/s3_pthread.h"
#include "lib/s3_socket.h"

#define MAX_EVENT 1024

/****************************destroy**************************/
static void s3_io_listen_destroy(S3Listen * l) {
    // TODO
}

static void s3_io_thread_destroy(S3IOThread *ioth) {
    // TODO
}

/**********************************s3 io***********************************/
S3IO *s3_io_construct() {
    S3IO *s3io = (S3IO *)s3_malloc(sizeof(S3IO));
    return s3io;
}

void s3_io_destroy(S3IO *s3io);

void s3_io_desconstruct(S3IO *s3io) {
    if (s3io != NULL) {
        s3_io_destroy(s3io);
        s3_free(s3io, sizeof(S3IO));
    }
}

int s3_io_init(S3IO *s3io, int io_thread_cnt) {
    assert(io_thread_cnt <= MAX_IO_THREAD_CNT);
    s3io->io_thread_cnt = io_thread_cnt;
    return 0;
}

void s3_io_destroy(S3IO *s3io) {
    printf("destroy s3io\n");
    s3_io_listen_destroy(&s3io->listen);
    for (int i = 0; i < s3io->io_thread_cnt; ++i) {
        s3_io_thread_destroy(s3io->ioths + i);
    }
}

/*********************************io routine**********************************/

static int s3_io_thread_init(S3IOThread *ioth, int id, S3IOHandler *handler) {
    ioth->id = id;
    ioth->handler = handler;
    pthread_spin_init(&ioth->pthread_lock, PTHREAD_PROCESS_PRIVATE);

    int ret = pipe(ioth->pipefd);
    assert(ret == 0);

    ioth->epfd = epoll_create1(0);
    assert(ioth->epfd >= 0);

    ioth->pipe_read_ev.events = EPOLLIN;
    ioth->pipe_read_ev.data.ptr = ioth;

    ret = epoll_ctl(ioth->epfd, EPOLL_CTL_ADD, ioth->pipefd[0], &ioth->pipe_read_ev);
    assert(ret == 0);

    return S3_OK;
}

static void s3_pipe_recv_socket_cb(S3IOThread *ioth) {

    int socket_fd;
    int ret = read(ioth->pipefd[0], &socket_fd, sizeof(int));
    assert(ret == sizeof(int));
#ifdef DEBUG
    printf("read socket fd=%d, dispatch io thread id=%d\n", socket_fd, ioth->id);
#endif

    S3Connection *c = s3_connection_construct();
    assert(c != NULL);
    s3_connection_init(c, ioth->epfd, socket_fd);
    c->handler = ioth->handler;
    c->ioth = ioth;

    ioth->conn_cnt++;

    // recv event
    c->ev.events = EPOLLIN;
    c->ev.data.ptr = c;
    ret = epoll_ctl(ioth->epfd, EPOLL_CTL_ADD, socket_fd, &c->ev);
    assert(ret == 0);
}

static void *s3_io_thread_start_rontine(void *arg) {
    S3IOThread *ioth = (S3IOThread*)arg;

    // TODO: cpu affinity

    int ret = S3_OK;
    struct epoll_event events[MAX_EVENT];

    int wait_cnt = 0;
    for (;;) {
        wait_cnt = epoll_wait(ioth->epfd, events, MAX_EVENT, -1);
        if(wait_cnt < 0){
            printf("epoll wait error. ret=%d, errno=%d\n", wait_cnt, errno);
            ret = wait_cnt;
            break;
        }

        for (int i = 0; i < wait_cnt; ++i) {
            uint32_t evts = events[i].events;

            if (evts & EPOLLERR || evts & EPOLLHUP) {
                void* ptr = events[i].data.ptr;
                S3Connection *c = (S3Connection*)ptr;
                printf("EPOLL event error routine. fd=%d, events=%d\n", c->fd, evts);
                c->closed = CONN_CLOSE_STATUS_COMPLETE;
                sleep(1);
                s3_io_connection_close(c);
                continue;
            } else if (evts & EPOLLIN) {
                // pipe fd, or connection fd
                void* ptr = events[i].data.ptr;
                if (ptr == (void*)ioth) {
                    s3_pipe_recv_socket_cb(ioth);
                } else {
                    S3Connection *c = (S3Connection*)ptr;
                    s3_connection_recv_socket_cb((S3Connection*)ptr);
                }
            } else if (evts & EPOLLOUT) {
                void* ptr = events[i].data.ptr;
                S3Connection *c = (S3Connection*)ptr;
                s3_connection_send_socket_cb((S3Connection*)ptr);
            }
        }
    }

    printf("io thread epoll run over, id=%d, ret=%d\n", ioth->id, ret);

    return NULL;
}

S3IO *s3_io_create(int io_thread_cnt, S3IOHandler *handler) {
    /*
     * 向收到RST的socket读写数据，会产生SIGPIPE信号，默认情况下会终止整个程序;
     * 设置忽略SIGPIPE后，向收到RST的socket读写数据会返回-1，并置errno=EPIPE;
     */
    signal(SIGPIPE, SIG_IGN);

    int ret = 0;
    S3IO *s3io = NULL;

    s3io = s3_io_construct();
    assert(s3io != NULL);
    ret = s3_io_init(s3io, io_thread_cnt);
    assert(ret == 0);

    for (int i = 0; i < s3io->io_thread_cnt; ++i) {
        S3IOThread *ioth = &s3io->ioths[i];
        s3_io_thread_init(ioth, i, handler);
        s3_pthread_create_joinable(&ioth->tid, s3_io_thread_start_rontine, (void*)ioth);
    }

    return s3io;
}

/*********************************listen**********************************/

static int s3_listen_init(S3Listen *l, S3IO *s3io) {
    int ret = S3_OK;

    l->epfd = epoll_create1(0);
    assert(l->epfd >= 0);

    l->listenfd = s3_socket_create_listenfd();
    assert(l->listenfd >= 0);

    l->ev.events = EPOLLIN;
    l->ev.data.ptr = s3io;

    ret = epoll_ctl(l->epfd, EPOLL_CTL_ADD, l->listenfd, &l->ev);
    assert(ret == 0);

    return S3_OK;
}

static void s3_accept_socket_cb(struct epoll_event *ev) {
    int fd;
    S3IO *s3io = (S3IO*)ev->data.ptr;
    int listenfd = s3io->listen.listenfd;

    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(struct sockaddr);
    do {
        fd = accept(listenfd, (struct sockaddr *)&sin, &addrlen);
        if (fd >= 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        perror("accept fail.");
        assert(false);
    } while (1);

#ifdef DEBUG
    printf("\n\naccept a connection. fd=%d\n", fd);
#endif

    S3IOThread *ioth = &s3io->ioths[s3io->recved_conn_cnt++ % s3io->io_thread_cnt];
    int ret = write(ioth->pipefd[1], &fd, sizeof(fd));
    assert(ret == sizeof(fd));
}

static void *s3_listen_thread_start_rontine(void *arg) {
    S3Listen *l = (S3Listen*)arg;
    int ret = S3_OK;
    struct epoll_event events[MAX_EVENT];

    int wait_cnt = 0;
    for (;;) {
        wait_cnt = epoll_wait(l->epfd, events, MAX_EVENT, -1);
        if(wait_cnt < 0){
            printf("epoll wait error. ret=%d, errno=%d\n", wait_cnt, errno);
            ret = wait_cnt;
            break;
        }

        for (int i = 0; i < wait_cnt; ++i) {
            uint32_t evts = events[i].events;

            // listen event, must be EPOLLIN
            if (evts & EPOLLERR || evts & EPOLLHUP || (!evts & EPOLLIN)) {
                printf("epoll event error listen. events=%d\n", evts);
                sleep(1);
                continue;
            }

            // listen event, event fd must be listenfd
            S3IO *s3io = (S3IO*)events[i].data.ptr;
            int listenfd = s3io->listen.listenfd;
            assert(l->listenfd == listenfd);

            s3_accept_socket_cb(events + i);
        }
    }

    printf("listen thread epoll run over, ret=%d\n", ret);

    return NULL;
}

void s3_io_start_run(S3IO *s3io) {
    int ret = s3_listen_init(&s3io->listen, s3io);
    assert(ret == 0);

    s3_pthread_create_joinable(&s3io->listen.tid, s3_listen_thread_start_rontine, (void*)&s3io->listen);
    printf("create listen routine, tid=%lu\n", s3io->listen.tid);
}

void s3_io_connection_close(void *conn) {
    S3Connection *c = (S3Connection*)conn;

    int ret = S3_OK;

    if (c->closed == CONN_CLOSE_STATUS_NONE) {
        c->closed = CONN_CLOSE_STATUS_HALF;
        /*
         * 支持流式上传，即上传之前不知道上传的文件长度。当客户端上传完毕关闭连接后
         * s3server需要尝试返回上传结果(fd半关闭时)
         */
        s3_connection_do_half_close(c);
    }

    s3_connection_try_destruct(c);
}
