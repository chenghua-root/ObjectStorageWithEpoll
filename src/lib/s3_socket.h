#ifndef S3_LIB_SOCKET_H_
#define S3_LIB_SOCKET_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

int s3_socket_set_non_blocking(int fd);
int s3_socket_create_listenfd();
int s3_socket_read(int fd, char *buf, int size);
int s3_socket_write(int fd, char *buf, int size);

#endif
