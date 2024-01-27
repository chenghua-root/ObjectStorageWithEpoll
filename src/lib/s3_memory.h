#ifndef S3_LIB_MEMORY_
#define S3_LIB_MEMORY_

#include <atomic>
#include <stdio.h>
#include <stdlib.h>

static std::atomic<long> s3_memory_inuse_size; // statistic for malloc & free

static __attribute__((constructor)) void __s3_memory_inuse_size_init() {
    std::atomic_init(&s3_memory_inuse_size, 0);
}

static long s3_memory_malloc_size() {
    return s3_memory_inuse_size.load();
}

static inline void* s3_malloc(ssize_t size) {
    s3_memory_inuse_size.fetch_add(size);
    return malloc(size);
}

static inline void s3_free(void *ptr, ssize_t size) {
    s3_memory_inuse_size.fetch_sub(size);
    free(ptr);
}


#endif

