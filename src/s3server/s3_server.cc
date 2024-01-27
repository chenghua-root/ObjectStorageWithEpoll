#include "s3_server.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/stat.h>

#include <leveldb/db.h>

#include "lib/s3_error.h"
#include "s3_global.h"
#include "s3_handle_request.h"

/***************************s3_init_datadir*************************/
int s3_init_datadir() {
    int ret = 0;

    struct stat st;
    if (stat(S3_DATA_DIR, &st) == -1) {
        ret = mkdir(S3_DATA_DIR, 0700);
        if (ret != 0) {
            printf("create data dir fail. %s\n", S3_DATA_DIR);
            return ret;
        }
    }

    if (stat(S3_DATA_BAK_DIR, &st) == -1) {
        ret = mkdir(S3_DATA_BAK_DIR, 0700);
        if (ret != 0) {
            printf("create data backup dir fail. %s\n", S3_DATA_BAK_DIR);
            return ret;
        }
    }

    return S3_OK;
}

/*****************************s3_init_db***************************/
int s3_init_meta_db(const std::string& dbname) {
    leveldb::DB *db;
    leveldb::Options options;
    options.create_if_missing = true;

    // open
    leveldb::Status status = leveldb::DB::Open(options, dbname, &db);
    assert(status.ok());

    s3_g.db = db;

    return S3_OK;
}

/*****************************s3_init_net***************************/
int s3_init_net(int io_thread_cnt) {
    S3IOHandler *io_handler = &s3_g.io_handler;
    io_handler->process = s3_handle_request;
    io_handler->close   = s3_io_connection_close;

    s3_g.s3io = s3_io_create(io_thread_cnt, io_handler);

    printf("init net succ...\n");

    return 0;
}

int s3_start_net() {
    s3_io_start_run(s3_g.s3io);

    return S3_OK;
}

/***************************s3_global_destroy*************************/
void s3_global_destroy() {
    s3_io_desconstruct(s3_g.s3io);
    s3_g.s3io = NULL;
}


/*******************************s3_signal*****************************/
static void s3_signal_handler_interrupt(int sig) {
    printf("receive SIGINT signal, signal num=%d\n", sig);
    s3_g.stop_flag = 1;
}

static void s3_signal_handler_quit(int sig) {
    printf("receive SIGTERM signal, signal num=%d\n", sig);
    s3_g.stop_flag = 1;
}

struct {
    int sig;
    __sighandler_t handler;
} s3_signal_table[] = {
    {SIGINT,  s3_signal_handler_interrupt},  // 2
    {SIGTERM, s3_signal_handler_quit},       // 15
};

int s3_regist_signal() {
    for (int i = 0; i < sizeof(s3_signal_table) / sizeof(s3_signal_table[0]); ++i) {
        typeof(s3_signal_table[0]) *sh = & s3_signal_table[i];
        if (SIG_ERR == signal(sh->sig, sh->handler)) {
            printf("regist signal fail. signal num = %d, errno = %s\n", sh->sig, strerror(errno));
            return S3_FAIL;
        }
    }
    return S3_OK;
}
