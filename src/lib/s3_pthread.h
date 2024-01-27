#ifndef S3_LIB_PTHREAD_H_
#define S3_LIB_PTHREAD_H_

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

typedef void* (*S3PthreadRoutine)(void *);
static inline int s3_pthread_create_(pthread_t *thread, S3PthreadRoutine routine, void *arg, int detachstate) {
  int ret = 0;
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  ret = pthread_attr_setdetachstate(&attr, detachstate);
  assert(ret == 0);
  ret = pthread_create(thread, &attr, routine, arg);
  assert(ret == 0);
  pthread_attr_destroy(&attr);

  return ret;
}

#define s3_pthread_create_joinable(thread, routine, arg) s3_pthread_create_((thread), (routine), (arg), PTHREAD_CREATE_JOINABLE)

#endif
