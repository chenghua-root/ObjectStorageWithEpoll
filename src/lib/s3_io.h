#ifndef S3_LIB_IO_H_
#define S3_LIB_IO_H_

#include <list>
#include <pthread.h>
#include <stdint.h>
#include <sys/epoll.h>

#include "lib/s3_connection.h"
#include "lib/s3_io_handler.h"

#define MAX_IO_THREAD_CNT 32

typedef struct S3Listen S3Listen;
struct S3Listen {
    pthread_t           tid;

    int                 listenfd;
    int                 epfd;
    struct epoll_event  ev;

};

typedef struct S3IOThread S3IOThread;
struct S3IOThread {
    pthread_t             tid;

    int                   id;
    S3IOHandler           *handler;
    pthread_spinlock_t    pthread_lock;

    int                   pipefd[2];

    int                   epfd;
    struct epoll_event    pipe_read_ev;

    uint64_t              conn_cnt; // TODO: manage connection
};

typedef struct S3IO S3IO;
struct S3IO {
    S3Listen        listen;
    uint64_t        recved_conn_cnt;

    int             io_thread_cnt;
    S3IOThread      ioths[MAX_IO_THREAD_CNT];
};

S3IO *s3_io_create(int io_thread_cnt, S3IOHandler *handler);
void s3_io_start_run(S3IO *s3io);
void s3_io_desconstruct(S3IO *s3io);

void s3_io_connection_close(void *c);

#endif
