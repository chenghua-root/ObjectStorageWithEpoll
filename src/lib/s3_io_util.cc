#include "s3_io_util.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "lib/s3_error.h"

int s3_io_util_create_file(char *path,int8_t sync) {
    int fd = -1;

    int mask = O_CREAT | O_EXCL | O_RDWR | O_TRUNC;
    if (sync) {
       mask |= O_SYNC;
    }
    fd = open(path, mask, 0640);
    if (fd < 0) {
        printf("create file fail. path=%s, errno=%d %s\n",
               path, errno, strerror(errno));
        return S3_ERR_IO;
    }
    return fd;
}

int s3_io_util_open_rw(char *path) {
    int fd = -1;

    fd = open(path, O_RDWR, 0640);
    if (fd < 0) {
      printf("open file fail. path=%s, errno=%d %s\n", path, errno, strerror(errno));
      return S3_ERR_IO;
    }
    return fd;
}

ssize_t s3_io_util_pread_buf(int fd, S3Buf *buf, int64_t offset) {
    assert(buf != NULL);
    assert(offset >= 0);

    int64_t l = s3_buf_free_size(buf);

    ssize_t ret = s3_io_util_pread(fd, buf->right, l, offset);
    buf->right += ret;

    return ret;
}

ssize_t s3_io_util_pread(int fd, char *buf, const int64_t buflen, int64_t offset) {
    assert(offset >= 0);

    ssize_t ret = 0;
    do {
        ret = pread(fd, buf, buflen, offset);
        if (ret < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            assert(0);
        }
        break;
    } while(1);

    return ret;
}

ssize_t s3_io_util_pwrite_buf(int fd, S3Buf *buf, int64_t offset) {
    assert(fd >= 0);
    assert(buf != NULL);

    int64_t l = s3_buf_unconsumed_size(buf);
    int ret = s3_io_util_pwrite(fd, buf->left, l, offset);
    assert(ret == S3_OK);
    buf->left += l;

    return l;
}

int s3_io_util_pwrite(int fd, char *buf, int64_t buflen, int64_t offset) {
    ssize_t nwritten = 0;
    char *p = buf;
    ssize_t left = buflen;

    while (left > 0) {
        nwritten = pwrite(fd, p, left, offset);
        if (nwritten >= 0) {
            left -= nwritten;
            p += nwritten;
            offset += nwritten;
        } else if (nwritten < 0) {
            switch (errno) {
                case EINTR:
                case EAGAIN:
                    continue;
                default:
                    printf("pwrite fail: fd=%d, offset=%lld, size=%lld, errno=%d %s\n",
                           fd, (long long)offset, (long long)buflen, errno, strerror(errno));
                    return S3_ERR_IO;
            }
        }
    }

    return S3_OK;
}
