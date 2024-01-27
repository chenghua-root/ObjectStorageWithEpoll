#ifndef S3_LIB_CONF_DEFINE_H_
#define S3_LIB_CONF_DEFINE_H_

#include <string>

#define PORT 9000
#define IP "0.0.0.0"

#define IO_THREAD_CNT 16

static std::string DB_NAME    = "/tmp/chenghua/entrytask/db";

static char S3_DATA_DIR[]     = "/tmp/chenghua/entrytask/data/";
static char S3_DATA_BAK_DIR[] = "/tmp/chenghua/entrytask/data/bak/";

static int S3_DATA_DIR_LEN = sizeof(S3_DATA_DIR) - 1;
static int S3_DATA_BAK_DIR_LEN = sizeof(S3_DATA_BAK_DIR) - 1;

#endif
