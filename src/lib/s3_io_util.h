#ifndef S3_LIB_IO_UTIL_
#define S3_LIB_IO_UTIL_

#include <stdint.h>
#include "sys/types.h"

#include "lib/s3_buf.h"

#define MAX_PATH_LEN 512

int s3_io_util_create_file(char *path, int8_t sync);
int s3_io_util_open_rw(char *path);

ssize_t s3_io_util_pread_buf(int fd, S3Buf *buf, int64_t offset);
ssize_t s3_io_util_pread(int fd, char *buf, const int64_t buflen, int64_t offset);

ssize_t s3_io_util_pwrite_buf(int fd, S3Buf *buf, int64_t offset);
int s3_io_util_pwrite(int fd, char *buf, int64_t buflen, int64_t offset);

#endif
