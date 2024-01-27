#ifndef S3_GLOBAL_H_
#define S3_GLOBAL_H_

#include <leveldb/db.h>

#include "lib/s3_io.h"
#include "lib/s3_io_handler.h"

typedef struct S3Global S3Global;
struct S3Global {
    S3IO                *s3io;
    S3IOHandler         io_handler;

    leveldb::DB         *db;

    int                 stop_flag;
};
#define s3_global_null {  \
    .s3io = NULL,         \
    .stop_flag = 0,       \
}

extern S3Global s3_g;

#endif
