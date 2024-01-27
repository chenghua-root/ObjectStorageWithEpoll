#ifndef S3_HANDLE_REQUEST_H_
#define S3_HANDLE_REQUEST_H_

#include <set>

#include "lib/s3_connection.h"

int s3_handle_request(S3Connection *c);

/*
 * 不支持同名并发上传
 */
static pthread_spinlock_t uploading_check_lock;
static std::set<std::string> uploading_check;

static __attribute__((constructor)) void __s3_uploading_check_lock_init() {
    pthread_spin_init(&uploading_check_lock, PTHREAD_PROCESS_PRIVATE);
}

static __attribute__((destructor)) void __s3_uploading_check_lock_destroy() {
    pthread_spin_destroy(&uploading_check_lock);
}

static bool check_same_name_uploading(char *file_name) {
    std::string file(file_name);

    pthread_spin_lock(&uploading_check_lock);
    auto it = uploading_check.find(file);
    if (it != uploading_check.end()) {
        pthread_spin_unlock(&uploading_check_lock);
        printf("check ture, file %s\n", file_name);
        return true;
    }

    uploading_check.insert(file);
    pthread_spin_unlock(&uploading_check_lock);
    printf("insert file %s\n", file_name);

    return false;
}

static void uploading_check_delete(char *file_name) {
    std::string file(file_name);

    pthread_spin_lock(&uploading_check_lock);
    uploading_check.erase(file);
    pthread_spin_unlock(&uploading_check_lock);
    printf("erase file %s\n", file_name);
}

#endif
