#include "lib/s3_object_req.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "lib/s3_error.h"

int s3_object_req_init(S3ObjectReq *req) {
    req->req_code = S3_OBJECT_CODE_START;
    req->step = S3_OBJECT_REQ_STEP_HEADER;
    req->file_fd = -1;
    req->offset = 0;
    req->file_len = 0;
    req->resped = false;

    return S3_OK;
}

void s3_object_req_destroy(S3ObjectReq *req) {
    req->req_code = S3_OBJECT_CODE_END;
    req->step = S3_OBJECT_REQ_STEP_END;
    close(req->file_fd);
}

void s3_object_req_header_decode(S3ObjectReq *req, S3Buf *buf) {
    assert(req != NULL);
    assert(buf != NULL);

    S3ObjectReqHeader header;
    header = *(S3ObjectReqHeader*)(buf->left);
    buf->left += S3_OBJECT_REQ_HEADER_SIZE;
    assert(header.req_code < S3_OBJECT_CODE_END);

    req->req_code = (S3_OBJECT_CODE)header.req_code;
    strcpy(req->file_name, header.file_name);

    memcpy(req->file_path, S3_DATA_DIR, S3_DATA_DIR_LEN);
    strcpy(req->file_path + S3_DATA_DIR_LEN, req->file_name);

    memcpy(req->file_bak_path, S3_DATA_BAK_DIR, S3_DATA_BAK_DIR_LEN);
    strcpy(req->file_bak_path + S3_DATA_BAK_DIR_LEN, req->file_name);

    req->step = S3_OBJECT_REQ_STEP_HEADER;
    req->file_fd = -1;
}
