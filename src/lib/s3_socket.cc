#include "lib/s3_socket.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "lib/s3_error.h"
#include "lib/s3_conf_define.h"

int s3_socket_set_non_blocking(int fd) {
    int flags = 1;
    return ioctl(fd, FIONBIO, &flags);
}

int s3_socket_create_listenfd() {
    struct sockaddr_in addr;
    int s;
    s = socket(AF_INET, SOCK_STREAM, 0);

    if(s == -1){
        perror("create socket error \n");
        return -1;
    }

    int so_reuseaddr = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr));
    bzero(&addr, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);

    if(bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1){
        perror("bind socket error \n");
        return -1;
    }

    if(listen(s, 32) == -1){
        perror("listen socket error\n");
        return -1;
    }
    printf("bind %s, listen %d\n", IP, PORT);

    return s;
}

int s3_socket_read(int fd, char *buf, int size) {
    int n = 0;
    do {
        n = recv(fd, buf, size, 0);
    } while (n == -1 && errno == EINTR);

    if (n < 0) {
        printf("socket read error. ret=%d, errno=%d %s\n", n, errno, strerror(errno));
        n = ((errno == EAGAIN) ? S3_ERR_NET_AGAIN : S3_ERR_NET_ABORT);
    }

    return n;
}

int s3_socket_write(int fd, char *buf, int size) {
    int n = 0;
    do {
        n = send(fd, buf, size, 0);
    } while (n == -1 && errno == EINTR);

    if (n < 0) {
        printf("socket write error. ret=%d, errno=%d %s\n", n, errno, strerror(errno));
        n = ((errno == EAGAIN) ? S3_ERR_NET_AGAIN : S3_ERR_NET_ABORT);
    }

    return n;
}
