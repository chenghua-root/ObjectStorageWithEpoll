#ifndef S3_LIB_BUF_H_
#define S3_LIB_BUF_H_

#include <stdint.h>

/* buf.data    buf.left      buf.right    buf.data+buf.len
 *  |               |             |            |
 *  +---------------+-------------+------------+
 *  |   consumed    |  unconsumed |     free   |
 *
 *  [buf.data, buf.right] has filled data,
 *  [buf.data, buf.left]  has consumed,
 *  [buf.left, buf.right] has not consumed.
 *
 *  consumed + unconsumed + free == buf.len
 */

struct S3Buf {
    int64_t len;
    char    *data;
    char    *left;
    char    *right;
};

#define s3_buf_null {          \
    .len = 0,                  \
    .data = NULL,              \
    .left = NULL,              \
    .right = NULL,             \
}

S3Buf *s3_buf_construct();
void s3_buf_destruct(S3Buf *b);
int s3_buf_init(S3Buf *b, int64_t len);
void s3_buf_destroy(S3Buf *b);

static inline int64_t s3_buf_consumed_size(const S3Buf *b) {
  return b->left - b->data;
}
static inline int64_t s3_buf_unconsumed_size(const S3Buf *b) {
  return b->right - b->left;
}
static inline int64_t s3_buf_free_size(const S3Buf *b) {
  return b->data + b->len - b->right;
}


#endif
