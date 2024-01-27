#include <atomic>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <pthread.h>

#include "lib/s3_connection.h"
#include "lib/s3_io_util.h"
#include "lib/s3_pthread.h"


#define MAX_THREAD_NUM    16
#define MAX_FILE_NAME_LEN 256
#define MAX_PATH_LEN      512

static std::atomic<int> performance_test_cnt{0};

struct S3Client {
    char file_path[MAX_PATH_LEN];
    char file_name[MAX_FILE_NAME_LEN];

    int  test_file_size;
    int  concurrent;
    int  total_test_cnt;
};

int parse_arg(int opt, S3Client &client, int argc, char *argv[]);

int put_object();
int get_object();
int del_object();

int put_file(S3Client *client);
int get_file(S3Client *client);
int del_file(S3Client *client);

void concurrent_test(S3Client *client);

int main(int argc, char *argv[]) {

    int opt = getopt(argc, argv, "hpgdta");
    if (opt == '?') {
        printf("invalid argument. try ./s3client -h\n");
        return -1;
    }

    if (opt == 'h' || opt == -1) {
        printf("\
Hello, this is simple storage server(S3) client for EntryTask!\n\n\
You could do put, get, delete file or do performance test.\n\
    PUT:  ./s3client -p file or path \n\
    GET:  ./s3client -g file [dest_dir] \n\
    DEL:  ./s3client -d file \n\
    TEST: ./s3client -t [file_size number_of_concurrent total_test_file_cnt] \n\
                        file_size:            max 1GB, default 128MB \n\
                        number_of_concurrent: max 16,  default 4 \n\
                        total_test_file_cnt:  max 100, default 16\n");
        return 0;
    }

    int ret = 0;
    S3Client client;
    if (opt == 'p') {
        parse_arg('p', client, argc, argv);
        ret = put_file(&client);
        return ret;
    } else if (opt == 'g') {
        parse_arg('g', client, argc, argv);
        ret = get_file(&client);
        return ret;
    } else if (opt == 'd') {
        parse_arg('d', client, argc, argv);
        ret = del_file(&client);
        return ret;
    } else if (opt == 't') {
        parse_arg('t', client, argc, argv);
        concurrent_test(&client);
        printf("\nPerformance Test. file size:%d, concurrent:%d, total file cnt:%d\n\n", client.test_file_size, client.concurrent, client.total_test_cnt);
        return 0;
    } else if (opt == 'a') {
        put_object();
        get_object();
        del_object();
        return 0;
    }

    return -1;
}

int parse_arg(int opt, S3Client &client, int argc, char *argv[]) {
    if (opt == 'p') {
        std::string path(argv[2]);
        strcpy(client.file_path, path.c_str());

        std::size_t found = path.find_last_of("/");
        if (found == std::string::npos) {
            strcpy(client.file_name, path.c_str());
        } else {
            std::string file = path.substr(found+1);
            strcpy(client.file_name, file.c_str());
        }
        printf("path=%s, file=%s\n", client.file_path, client.file_name);

        return 0;
    }

    if (opt == 'g') {
        std::string file_name(argv[2]);
        strcpy(client.file_name, file_name.c_str());

        std::string path = "";
        if (argc >= 4) {
            path = argv[3];
            path.append("/");
        }
        path.append(file_name);
        strcpy(client.file_path, path.c_str());

        printf("file path=%s\n", client.file_path);

        return 0;
    }

    if (opt == 'd') {
        std::string file_name(argv[2]);
        strcpy(client.file_name, file_name.c_str());

        return 0;
    }

    if (opt == 't') {
        if (argc >= 5) {
            std::string test_file_size(argv[2]);
            std::string concurrent(argv[3]);
            std::string total_test_cnt(argv[4]);

            client.test_file_size = std::stoi(test_file_size);
            client.concurrent = std::stoi(concurrent);
            client.total_test_cnt = std::stoi(total_test_cnt);

            if (client.test_file_size > 1024 * 1024 * 1024
                || client.concurrent > 16
                || client.total_test_cnt > 100) {
                printf("invalid arg...\n");
                return 1;
            }
        } else {
            client.test_file_size = 128 * 1024 * 1024;
            client.concurrent = 4;
            client.total_test_cnt = 16;
        }
        return 0;
    }

    return -1;
}

#define err_message(msg) \
    do {perror(msg); exit(EXIT_FAILURE);} while(0)

static int create_clientfd(char const *addr, uint16_t u16port) {
    int fd;
    struct sockaddr_in server;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) err_message("socket err\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(u16port);
    inet_pton(AF_INET, addr, &server.sin_addr);

    if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) perror("connect err\n");

    return fd;
}

#define BUF_LEN 2 * 1024 * 1024

int del_object() {
    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_DELETE)};
    char file_name[] = "test256";
    strcpy(header.file_name, file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    S3ObjectRespHeader resp_header;

    char buf[BUF_LEN] = {0};
    int n = read(fd, buf, S3_OBJECT_RESP_HEADER_SIZE);
    assert(n == S3_OBJECT_RESP_HEADER_SIZE);
    memcpy(&resp_header, buf, S3_OBJECT_RESP_HEADER_SIZE);
    printf("DEL. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

    return 0;
}

int get_object() {
    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_GET)};
    char file_name[] = "test256";
    strcpy(header.file_name, file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    S3ObjectRespHeader resp_header;

    char buf[BUF_LEN] = {0};
    int n = read(fd, buf, S3_OBJECT_RESP_HEADER_SIZE);
    assert(n == S3_OBJECT_RESP_HEADER_SIZE);
    memcpy(&resp_header, buf, S3_OBJECT_RESP_HEADER_SIZE);
    printf("GET. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

    uint64_t offset = 0;
    do {
        n = read(fd, buf, BUF_LEN);
        offset += n;
        if (n == 0) {
            close(fd);
            break;
        }
    } while(1);
    printf("------file len=%ld\n", offset);

    return 0;
}

int put_object() {

    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_PUT)};

    char file_name[] = "test256";
    strcpy(header.file_name, file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    char buf[BUF_LEN] = {0};
    for (int i = 0; i < BUF_LEN; ++i) {
        buf[i] = 'A' + i % 26;
        if (i%100 == 99) {
            buf[i] = '\n';
        }
    }

    int file_size = 256 * 1024 * 1024;
    int left_size = file_size;
    while (left_size > 0) {
        int size = BUF_LEN;
        if (left_size < size) {
            size = left_size;
        }
        int n = write(fd, buf, size);
        assert(n >= 0);
        left_size -= n;
    }
    printf("------write len=%d\n", file_size);

    shutdown(fd, SHUT_WR);

    S3ObjectRespHeader resp_header;
    ret = read(fd, &resp_header, S3_OBJECT_RESP_HEADER_SIZE);
    printf("PUT. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

    return 0;
}

int put_file(S3Client *client) {
    int file_fd = s3_io_util_open_rw(client->file_path);
    if (file_fd < 0) {
        printf("open file fail. file may not exist\n");
        return file_fd;
    }

    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_PUT)};
    strcpy(header.file_name, client->file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    char buf[BUF_LEN] = {0};
    int offset = 0;
    do {
        int n = s3_io_util_pread(file_fd, buf, BUF_LEN, offset);
        if (n <= 0) {
            break;
        }
        int left = n;
        while (left > 0) {
            int size = n;
            if (left < size) {
                size = left;
            }
            int ret = write(fd, buf, size);
            assert(ret >= 0);
            left -= ret;
        }
        offset += n;
    } while(1);

    shutdown(fd, SHUT_WR);

    S3ObjectRespHeader resp_header;
    ret = read(fd, &resp_header, S3_OBJECT_RESP_HEADER_SIZE);
    printf("PUT. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

    close(fd);

    return 0;
}

int get_file(S3Client *client) {
    int file_fd = s3_io_util_create_file(client->file_path, 0);
    if (file_fd < 0) {
        return -1;
    }

    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_GET)};
    strcpy(header.file_name, client->file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    S3ObjectRespHeader resp_header;

    char buf[BUF_LEN] = {0};
    int n = read(fd, buf, S3_OBJECT_RESP_HEADER_SIZE);
    assert(n == S3_OBJECT_RESP_HEADER_SIZE);
    memcpy(&resp_header, buf, S3_OBJECT_RESP_HEADER_SIZE);
    printf("GET. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);
    if (resp_header.status_code == S3_OBJECT_STATUS_CODE_NOT_EXIST) {
        printf("file %s not exist\n", header.file_name);
        return -1;
    }
    if (resp_header.status_code != S3_OBJECT_STATUS_CODE_SUCC) {
        printf("get file %s, server internal error\n", header.file_name);
        return -1;
    }

    uint64_t offset = 0;
    do {
        n = read(fd, buf, BUF_LEN);
        if (n == 0) {
            close(fd);
            break;
        }

        ret = s3_io_util_pwrite(file_fd, buf, n, offset);
        assert(ret == 0);

        offset += n;
    } while(1);
    printf("read object length=%ld\n", offset);

    return 0;
}

int del_file(S3Client *client) {
    int fd;
    fd = create_clientfd(IP, PORT);

    S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_DELETE)};
    strcpy(header.file_name, client->file_name);

    int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
    assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

    S3ObjectRespHeader resp_header;

    char buf[BUF_LEN] = {0};
    int n = read(fd, buf, S3_OBJECT_RESP_HEADER_SIZE);
    assert(n == S3_OBJECT_RESP_HEADER_SIZE);
    memcpy(&resp_header, buf, S3_OBJECT_RESP_HEADER_SIZE);
    printf("DEL. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
           resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

    return 0;
}

#define MEM_BUF_LEN 16 * 1024 * 1024
char *mem_buf = NULL;

static void mem_buf_init() {
    mem_buf = (char*) malloc(MEM_BUF_LEN);
    assert(mem_buf != NULL);

    for (int i = 0; i < MEM_BUF_LEN; ++i) {
        mem_buf[i] = 'A' + i % 26;
        if (i%100 == 99) {
            mem_buf[i] = '\n';
        }
    }
}

void *concurreent_test_routine(void *client_) {
    assert(client_ != NULL);

    S3Client *client = (S3Client*)client_;

    while (1) {
        int idx = performance_test_cnt.fetch_add(1, std::memory_order_seq_cst);
        if (idx >= client->total_test_cnt) {
            break;
        }

        char file_name[MAX_FILE_NAME];
        sprintf(file_name, "%d_%d", client->test_file_size, idx);
#ifdef DEBUG
        printf("file_name=%s, file len=%d\n", file_name, client->test_file_size);
#endif

        int fd;
        fd = create_clientfd(IP, PORT);

        S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_PUT)};
        strcpy(header.file_name, file_name);

        int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
        assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

        int file_size = client->test_file_size;
        int left_size = file_size;
        while (left_size > 0) {
            int size = MEM_BUF_LEN;
            if (left_size < size) {
                size = left_size;
            }
            int n = write(fd, mem_buf, size);
            assert(n >= 0);
            left_size -= n;
        }
        shutdown(fd, SHUT_WR);

        S3ObjectRespHeader resp_header;
        ret = read(fd, &resp_header, S3_OBJECT_RESP_HEADER_SIZE);
#ifdef DEBUG
        printf("PUT. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
               resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);
#endif
    }

    return NULL;
}

void concurrent_test_delete_files(S3Client *client) {

    for (int idx = 0; idx < client->total_test_cnt; ++idx) {

        char file_name[MAX_FILE_NAME];
        sprintf(file_name, "%d_%d", client->test_file_size, idx);
        printf("file_name=%s, file len=%ld\n", file_name, strlen(file_name));

        int fd;
        fd = create_clientfd(IP, PORT);

        S3ObjectReqHeader header = {.req_code = uint8_t(S3_OBJECT_CODE_DELETE)};
        strcpy(header.file_name, file_name);

        int ret = write(fd, &header, S3_OBJECT_REQ_HEADER_SIZE);
        assert(ret == S3_OBJECT_REQ_HEADER_SIZE);

        S3ObjectRespHeader resp_header;

        char buf[BUF_LEN] = {0};
        int n = read(fd, buf, S3_OBJECT_RESP_HEADER_SIZE);
        assert(n == S3_OBJECT_RESP_HEADER_SIZE);
        memcpy(&resp_header, buf, S3_OBJECT_RESP_HEADER_SIZE);
        printf("DEL. file name =%s, resp_code=%d, status_code=%d, file_len=%ld\n",
               resp_header.file_name, resp_header.resp_code, resp_header.status_code, resp_header.file_len);

        close(fd);
    }
}

static inline int64_t s3_get_timestamp() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return ((int64_t)(t.tv_sec) * (int64_t)(1000000) + (int64_t)(t.tv_usec));
}

void concurrent_test(S3Client *client) {
    assert(client != NULL);

    mem_buf_init();

    int64_t start_ts = s3_get_timestamp();

    pthread_t threads[MAX_THREAD_NUM] = {0};
    for (int i = 0; i < client->concurrent; ++i) {
        pthread_create(&threads[i], NULL, concurreent_test_routine, (void*)client);
    }
    for (int i = 0; i < client->concurrent; ++i) {
      pthread_join(threads[i], NULL);
    }

    int64_t used_ts = s3_get_timestamp() - start_ts;

    concurrent_test_delete_files(client);

    printf("\nTotal spend %ld us(not include delete file), average upload file spend time (spend time/(total_test_cnt/concurrent)) = %fus\n",
           used_ts, used_ts/(client->total_test_cnt / float(client->concurrent)));
}
