#ifndef S3_SERVER_H_
#define S3_SERVER_H_

#include <string>

int s3_init_datadir();

int s3_init_meta_db(const std::string& dbname);

int s3_init_net(int io_thread_cnt);

int s3_start_net();

void s3_global_destroy();

int s3_regist_signal();

#endif
