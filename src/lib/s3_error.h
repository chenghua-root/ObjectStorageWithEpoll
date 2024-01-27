#ifndef S3_LIB_ERROR_H_
#define S3_LIB_ERROR_H_

enum S3Error {
    S3_OK = 0,
    S3_ERR_ERR = -1000,
    S3_ERR_NET_AGAIN,
    S3_ERR_NET_ABORT,
    S3_ERR_IO,
    S3_ERR_INVALID_RPC,
};

#define S3_FAIL S3_ERR_ERR

#endif
