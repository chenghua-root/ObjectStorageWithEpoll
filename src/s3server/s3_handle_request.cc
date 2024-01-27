#include "s3_handle_request.h"

#include <iostream>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lib/s3_error.h"
#include "lib/s3_io.h"
#include "lib/s3_io_util.h"
#include "s3_global.h"

static leveldb::Status s3_db_put(char *key_buf, int value);
static leveldb::Status s3_db_get(char *key_buf, int &value);
static leveldb::Status s3_db_delete(char *key_buf);

static void s3_put_rename_if_exist(S3ObjectReq *req);

static int s3_handle_request_put(S3Connection *c);
static int s3_handle_request_get(S3Connection *c);
static int s3_handle_request_delete(S3Connection *c);

/***************************run by io thread***********************************/
int s3_handle_request(S3Connection *c) {
    int ret = S3_OK;

    int req_code = c->object_req.req_code;

    switch (req_code) {
        case S3_OBJECT_CODE_PUT:
            ret = s3_handle_request_put(c);
            break;
        case S3_OBJECT_CODE_GET:
            ret = s3_handle_request_get(c);
            break;
        case S3_OBJECT_CODE_DELETE:
            ret = s3_handle_request_delete(c);
            break;
        default:
            printf("invalid req code=%d\n", req_code);
            return S3_ERR_INVALID_RPC;
    }
    return ret;
}

static int s3_handle_request_put(S3Connection *c) {
    S3ObjectReq* req = &c->object_req;
    if (req->step == S3_OBJECT_REQ_STEP_HEADER) {
        do {
            bool uploading = check_same_name_uploading(req->file_name);
            if (uploading) {
                printf("same name file is uploading, file=%s\n", req->file_name);
                req->step = S3_OBJECT_REQ_STEP_END;
                c->status_code = S3_OBJECT_STATUS_CODE_FAIL;
                break;
            }

            s3_put_rename_if_exist(req);

            assert(req->file_fd == -1);
            int fd = s3_io_util_create_file(req->file_path, 0);
            if (fd < 0) {
                printf("concurrent put with same file name.");
                req->step = S3_OBJECT_REQ_STEP_END;
                c->status_code = S3_OBJECT_STATUS_CODE_FAIL;
                break;
            }

            req->file_fd = fd;
            req->step = S3_OBJECT_REQ_STEP_BODY;
            break;
        } while(1);
    }

    if (req->step == S3_OBJECT_REQ_STEP_BODY) {
        ssize_t ret = s3_io_util_pwrite_buf(req->file_fd, c->recv_buf, req->offset);
        assert(ret >= 0);
        req->offset += ret;
    }

    if (req->step == S3_OBJECT_REQ_STEP_END) {
        if (c->status_code == S3_OBJECT_STATUS_CODE_SUCC) {
            req->file_len = req->offset;
            leveldb::Status status = s3_db_put(req->file_name, req->file_len);
            assert(status.ok());
        }

        uploading_check_delete(req->file_name);

        s3_connection_do_resp(c);

        /*
         * 上传文件的结束靠客户端close/shutdown connection驱动,
         * connection收到close时会判定当前执行的step，若不为end,
         * 会调用process，然后自动close connection.
         * 此处不需要主动close.
         */
        if (c->status_code != S3_OBJECT_STATUS_CODE_SUCC) {
            s3_io_connection_close(c);
        }
    }

    return S3_OK;
}

static int s3_handle_request_get(S3Connection *c) {
    S3ObjectReq* req = &c->object_req;
    if (req->step == S3_OBJECT_REQ_STEP_HEADER) {
        int file_len;
        leveldb::Status st = s3_db_get(req->file_name, file_len);
        if (!st.ok()) {
            if (st.IsNotFound()) {
                c->status_code = S3_OBJECT_STATUS_CODE_NOT_EXIST;
            } else {
                c->status_code = S3_OBJECT_STATUS_CODE_FAIL;
            }
            req->step = S3_OBJECT_REQ_STEP_END;
        } else {
            assert(req->file_fd == -1);
            int fd = s3_io_util_open_rw(req->file_path);
            assert(fd >= 0);
            req->file_fd = fd;

            req->file_len = file_len;
            req->step = S3_OBJECT_REQ_STEP_BODY;
        }
        s3_connection_do_resp(c);
    }

    if (req->step == S3_OBJECT_REQ_STEP_BODY) {
        ssize_t n = s3_io_util_pread_buf(req->file_fd, c->send_buf, req->offset);
        assert(n >= 0);
        req->offset += n;
        s3_connection_send_msg_buf(c);

        if (n == 0) {
            req->step = S3_OBJECT_REQ_STEP_END;
        }
    }

    if (req->step == S3_OBJECT_REQ_STEP_END) {
        s3_io_connection_close(c);
    }

    return S3_OK;
}

static int s3_handle_request_delete(S3Connection *c) {
    S3ObjectReq* req = &c->object_req;
    if (req->step == S3_OBJECT_REQ_STEP_HEADER) {
        int file_len;
        leveldb::Status st = s3_db_get(req->file_name, file_len);
        if (!st.ok()) {
            if (st.IsNotFound()) {
                c->status_code = S3_OBJECT_STATUS_CODE_NOT_EXIST;
            } else {
                c->status_code = S3_OBJECT_STATUS_CODE_FAIL;
            }
        } else {
            req->file_len = file_len;
            st = s3_db_delete(req->file_name);
            assert(st.ok());
            int ret = remove(req->file_path);
            if (ret != 0) {
                printf("DELETE: file not exist\n");
                c->status_code = S3_OBJECT_STATUS_CODE_NOT_EXIST;
            } else  {
                c->status_code = S3_OBJECT_STATUS_CODE_SUCC;
            }
        }
        req->step = S3_OBJECT_REQ_STEP_END;
    }

    if (req->step == S3_OBJECT_REQ_STEP_END) {
        s3_connection_do_resp(c);
        s3_io_connection_close(c);
    }

    return S3_OK;
}

static void s3_put_rename_if_exist(S3ObjectReq *req) {
    int ret = 0;

    if (access(req->file_path, F_OK) == 0) {
#ifdef DEBUG
        printf("PUT FILE. file exist=%s\n", req->file_path);
#endif

        if (access(req->file_bak_path, F_OK) == 0) {
            ret = remove(req->file_bak_path);
            assert(ret == 0);
        }

        ret = rename(req->file_path, req->file_bak_path);
        assert(ret == 0);
    }
}

static leveldb::Status s3_db_put(char *key_buf, int value) {
    std::string key(key_buf);
    std::string str_value = std::to_string(value);

    leveldb::Status st = s3_g.db->Put(leveldb::WriteOptions(), key, str_value);
    return st;
}

static leveldb::Status s3_db_get(char *key_buf, int &value) {
    std::string key(key_buf);
    std::string str_value;

    leveldb::Status st = s3_g.db->Get(leveldb::ReadOptions(), key, &str_value);
    if (st.ok()) {
        value = std::stoi(str_value);
    }
    return st;
}

static leveldb::Status s3_db_delete(char *key_buf) {
    std::string key(key_buf);

    leveldb::Status st = s3_g.db->Delete(leveldb::WriteOptions(), key);
    return st;
}

