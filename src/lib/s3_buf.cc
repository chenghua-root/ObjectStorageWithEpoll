#include "lib/s3_buf.h"

#include <stdlib.h>
#include <assert.h>
#include "lib/s3_error.h"
#include "lib/s3_memory.h"

S3Buf *s3_buf_construct() {
    S3Buf *b = (S3Buf*)s3_malloc(sizeof(S3Buf));
    if (b != NULL) {
        *b = (S3Buf)s3_buf_null;
    }
    return b;
}

void s3_buf_destruct(S3Buf *b) {
    if (b != NULL) {
        s3_buf_destroy(b);
        s3_free(b, sizeof(S3Buf));
    }
}

int s3_buf_init(S3Buf *b, int64_t len) {
    b->data = (char*)s3_malloc(len);
    assert(b->data != NULL);
    b->left = b->right = b->data;
    b->len = len;

    return S3_OK;
}

void s3_buf_destroy(S3Buf *b) {
    if (b != NULL) {
        s3_free(b->data, b->len);
        *b = (S3Buf)s3_buf_null;
    }
}
