#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/s3_error.h"
#include "lib/s3_memory.h"
#include "lib/s3_conf_define.h"
#include "s3_global.h"
#include "s3_server.h"

int main(int argc, char *argv[]) {

    printf("Hello, this is simple storage server(S3) for EntryTask!\n");

    int ret = S3_OK;

    ret = s3_init_datadir();
    if (ret != S3_OK) {
        perror("init data dir fail");
        exit(1);
    }

    ret = s3_init_meta_db(DB_NAME); // leveldb
    if (ret != S3_OK) {
        perror("init db fail");
        exit(1);
    }

    ret = s3_init_net(IO_THREAD_CNT);
    if (ret != S3_OK) {
        perror("init net fail");
        exit(1);
    }

    ret = s3_start_net();
    if (ret != S3_OK) {
        perror("start net fail");
        exit(1);
    }

    ret = s3_regist_signal();
    if (ret != S3_OK) {
        perror("regist signal fail");
        exit(1);
    }

    printf("init succ, memory malloc inuse size = %ld\n", s3_memory_malloc_size());

    while(s3_g.stop_flag == 0) {
        sleep(1);
    }

    s3_global_destroy();

    long memory_inuse_size = s3_memory_malloc_size();
    printf("stop server, memory malloc inuse size = %ld\n", s3_memory_malloc_size());

    return 0;
}
