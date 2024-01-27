## EntryTaskObjectStorage

This is an EntryTask in my work, creating a object storage system in C/C++.

### Features
Create a object storage system in C/C++, and implement following functions:

- **A TCP server based on epoll event loop**.

- Support for concurrent object upload, download and delete.

- Separating object data from object metadata in storage system for robustness and scalability.

- Use key-value store to manage object metadata, e.g. object name.

- Use local file system to store object data.

- C/C++ Client APIs to provide PUT, GET and DELETE operations for object access.

### Infrastructure

- The service and its dependencies should be run in the VirtualDev/VM (Ubuntu 16.04).

- **Use standard library whenever possible**.

## Compile

sudo apt install -y libleveldb-dev

Notice: modify IP (in src/lib/s3_conf_define.h) to Server's IP before compile, if client and server run in different nodes.

./autobuild.sh                     // compile with debug info

./autobuild.sh release or Release  // compile with no debug info

## Run

### Server

./bin/s3server

Data and meta would store in /tmp/chenghua/entrytask/, which is configed in src/lib/s3_conf_define.h

### Client

./bin/s3client -h

## Performance Test

### Test Env

CPU: 48 * 2.4GB

Memory: 128GB

Disk: One SSD(System Disk)

FileSystem: Ext4. /tmp/chenghua/entrytask/

### Test

Compile with release model: ./autobuild.sh release

Server and client are in same machine. Server has 16 IO threads.

./bin/s3client -t file_size 16 total_test_cnt // Fixed 16 concurrent.

Average upload one file spend time (spend time/(total_test_cnt/concurrent)):

```
 size     time    concurrenat  total_test_cnt
  1GB:   30705ms       16          32
128MB:     486ms       16          64
  1MB:     3.6ms       16          128
 64KB:     2.2ms       16          1024
```

## System Design

### Connection Model

System is based on epoll and socket.

There is one ListenEpoll and multi IOEpoll. every epoll instance belong to a pthread.

When a connection coming, ListenEpoll accepts the connection, and distribute it to one IOEpoll with round robin.

An IOEpoll with pthread could service multi connections in same time.

### RPC Model

Connection do not packet the data(net stream), so one connection can only service one request all life time.

### Thread Model

There is one listen thread and multi IO thread, no other thread.

An request's all step run in one IO thread, include reading packet, process, response...

### Request Protocol

Every request should send an request header.

Becasue system would support upload with an stream/Buffer, server do not know upload finish offset, should driven by client close/shutdown the connection.

```
PUT:
     Client               Server
     ReqHeader --------->
                          Decode request header
     Data      --------->
      ...
     Fin(Close)--------->
                          Upload complete, store the meta in leveldb

GET:
     Client               Server
     ReqHeader --------->
                          Decode request header

               <--------- RespHeader // file exist or not exist
     Decode header
               <--------- Data
                          ...
               <--------- Fin(Close)

DELETE:
     Client               Server
     ReqHeader --------->
                          Decode request header

                          Delete file if exist

               <--------- RespHeader // delete succ or fail
     Decode header
```

## Consistency

支持并发上传和覆盖上传，但不支持同名并发上传.

一致性依赖元数据。

单个上传:先存储数据(本地文件), 在存储元数据。成功与否与元数据保持一致。

并发上传：一致性与单个上传一致。一致性依赖元数据。

覆盖上传：先备份旧文件数据(rename)，再上传新文件，在存储新文件元数据。上传失败时，可以把备份恢复，恢复暂未支持.

同名并发上传：禁止。

## Consistency Optimize

上传时可以存储为一个全局唯一的文件名(如IP+本机时间戳(保证唯一递增))，当文件上传完成后，再原子的替换元数据，再删除旧文件。

元数据存储{ObjectName, LocalFileName, ...}
