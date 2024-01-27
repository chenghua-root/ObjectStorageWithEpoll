#ifndef S3_LIB_OBJECT_REQ_H_
#define S3_LIB_OBJECT_REQ_H_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "lib/s3_buf.h"
#include "lib/s3_conf_define.h"

#define MAX_FILE_NAME 256
#define MAX_PATH_LEN 512

enum S3_OBJECT_CODE {
    S3_OBJECT_CODE_START       = 100,

    S3_OBJECT_CODE_PUT         = 101,
    S3_OBJECT_CODE_PUT_RESP    = 102,

    S3_OBJECT_CODE_GET         = 103,
    S3_OBJECT_CODE_GET_RESP    = 104,

    S3_OBJECT_CODE_DELETE      = 105,
    S3_OBJECT_CODE_DELETE_RESP = 106,

    S3_OBJECT_CODE_END         = 107,
};

enum S3_OBJECT_REQ_STEP {
    S3_OBJECT_REQ_STEP_HEADER = 0,
    S3_OBJECT_REQ_STEP_BODY   = 1,
    S3_OBJECT_REQ_STEP_END    = 2,
};

enum S3_OBJECT_STATUS_CODE {
    S3_OBJECT_STATUS_CODE_SUCC      = 0,
    S3_OBJECT_STATUS_CODE_FAIL      = 1,
    S3_OBJECT_STATUS_CODE_EXISTED   = 2,
    S3_OBJECT_STATUS_CODE_NOT_EXIST = 3,
};

struct S3ObjectReqHeader {
    uint8_t req_code;
    uint8_t file_name_len;
    char    file_name[MAX_FILE_NAME];
};
static int S3_OBJECT_REQ_HEADER_SIZE = sizeof(S3ObjectReqHeader);

struct S3ObjectRespHeader {
    uint8_t  resp_code;
    uint8_t  status_code;
    uint64_t file_len;
    char     file_name[MAX_FILE_NAME];
};
static int S3_OBJECT_RESP_HEADER_SIZE = sizeof(S3ObjectRespHeader);

struct S3ObjectReq {
    S3_OBJECT_CODE       req_code;
    S3_OBJECT_REQ_STEP   step;

    int                  file_name_len;
    char                 file_name[MAX_FILE_NAME];

    char                 file_path[MAX_PATH_LEN];
    char                 file_bak_path[MAX_PATH_LEN];

    int                  file_fd;
    uint64_t             offset;
    uint64_t             file_len;

    bool                 resped;
};

int s3_object_req_init(S3ObjectReq *req);
void s3_object_req_destroy(S3ObjectReq *req);
void s3_object_req_header_decode(S3ObjectReq *req, S3Buf *buf);

#endif
