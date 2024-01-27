## EPOLL

### EPOLL EVENTS

https://sites.uclouvain.be/SystInfo/usr/include/sys/epoll.h.html

```
enum EPOLL_EVENTS
{
    EPOLLIN = 0x001,
    EPOLLPRI = 0x002,
    EPOLLOUT = 0x004,
    EPOLLERR = 0x008,
    EPOLLHUP = 0x010,
    EPOLLRDNORM = 0x040,
    EPOLLRDBAND = 0x080,
    EPOLLWRNORM = 0x100,
    EPOLLWRBAND = 0x200,
    EPOLLMSG = 0x400,
    EPOLLRDHUP = 0x2000,
    EPOLLONESHOT = (1 << 30),
    EPOLLET = (1 << 31)
};
```

### EPOLLOUT
https://blog.csdn.net/dashoumeixi/article/details/94673554

## SOCKET状态

全关闭：close(fd)
半关闭：shutdown(fd, WR)，不可写，可读

close(fd)后本端读写fd报错，errno = 9(EBADF), Bad File Descriptor
close(fd)后对端写数据，errno = 104(ECONNRESET), connection reset by peer

读写失败时，若errno是如下两种，重试即可:
- EINTR: retry at once
- EAGAIN: retry next time

向收到RST的socket读写数据，会产生SIGPIPE信号，默认情况下会终止整个程序;
设置忽略SIGPIPE后，向收到RST的socket读写数据会返回-1，并置errno=EPIPE;

怎么感知socket已关闭或reset:
read() 返回0，表示对端关闭了写，通过close或shutdown
read() 返回-1，且errno不是EINTR或EAGAIN，表示socket收到了reset，如对端进程退出（EPIPE）；或本端已执行close（EBADF）
write()返回0，表示写满了
write()返回-1，且errno不是EINTR或EAGIN，表示socket收到了reset，如对端进程退出（EPIPE）；或对端已经close了连接（ECONNRESET）；或本端已执行close（EBADF）
