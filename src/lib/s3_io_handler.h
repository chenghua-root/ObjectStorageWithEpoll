#ifndef S3_LIB_IO_HANDLER_H_
#define S3_LIB_IO_HANDLER_H_

typedef struct S3IOHandler S3IOHandler;
struct S3IOHandler {
  int       (*process)(struct S3Connection *c);
  void      (*close)  (void *conn);

  //int       (*epoll_ctl_)(void *ioth, int op, int fd, struct epoll_event *event);
};

#endif
