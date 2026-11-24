#ifndef EXODUS_HORIZON_SOCKET_SMOKE_H
#define EXODUS_HORIZON_SOCKET_SMOKE_H
/* Local diagnostic only. No LAN bind, remote requests or credentials. */
#include <switch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static inline int exn_nonblock(int fd) {
    int flags=fcntl(fd,F_GETFL,0);
    return flags>=0 && fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;
}
static inline int exn_ready(int fd,short events) {
    struct pollfd p={.fd=fd,.events=events};
    return poll(&p,1,1000)==1 && (p.revents&events) &&
           !(p.revents&(POLLERR|POLLHUP|POLLNVAL));
}
/* Fixed 8-byte payload and bounded attempts, independent of peer behavior. */
static inline int exn_transfer(int fd,unsigned char *b,size_t n,int writing) {
    size_t off=0;
    for(unsigned i=0;i<8 && off<n;++i){
        if(!exn_ready(fd,writing?POLLOUT:POLLIN))return 0;
        ssize_t rc=writing?send(fd,b+off,n-off,0):recv(fd,b+off,n-off,0);
        if(rc<=0 || (size_t)rc>n-off)return 0;
        off+=(size_t)rc;
    }
    return off==n;
}
/* Returns stage number on failure, zero on success. Owns socket lifecycle. */
static inline int exn_loopback_smoke(void) {
    int server=-1,client=-1,accepted=-1,stage=1;
    if(R_FAILED(socketInitializeDefault()))return stage;
    struct sockaddr_in a={0},peer={0};
    socklen_t len=sizeof(a),plen=sizeof(peer);
    a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=0;
    stage=2;server=socket(AF_INET,SOCK_STREAM,0);
    if(server<0 || !exn_nonblock(server))goto done;
    stage=3;
    if(bind(server,(struct sockaddr*)&a,sizeof(a)) ||
       listen(server,1) || getsockname(server,(struct sockaddr*)&a,&len) ||
       a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK) || !a.sin_port)goto done;
    stage=4;client=socket(AF_INET,SOCK_STREAM,0);
    if(client<0 || !exn_nonblock(client))goto done;
    int rc=connect(client,(struct sockaddr*)&a,sizeof(a));
    if(rc<0 && errno!=EINPROGRESS && errno!=EWOULDBLOCK)goto done;
    stage=5;
    if(!exn_ready(server,POLLIN))goto done;
    accepted=accept(server,(struct sockaddr*)&peer,&plen);
    if(accepted<0 || !exn_nonblock(accepted) ||
       peer.sin_family!=AF_INET || peer.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))goto done;
    stage=6;
    int error=0;socklen_t elen=sizeof(error);
    if(!exn_ready(client,POLLOUT) ||
       getsockopt(client,SOL_SOCKET,SO_ERROR,&error,&elen) || error)goto done;
    stage=7;
    unsigned char sent[8]={'E','X','R','S','M','O','K','E'},recvbuf[8]={0};
    if(!exn_transfer(client,sent,sizeof(sent),1) ||
       !exn_transfer(accepted,recvbuf,sizeof(recvbuf),0) ||
       memcmp(sent,recvbuf,sizeof(sent)))goto done;
    stage=8;memset(recvbuf,0,sizeof(recvbuf));
    if(!exn_transfer(accepted,sent,sizeof(sent),1) ||
       !exn_transfer(client,recvbuf,sizeof(recvbuf),0) ||
       memcmp(sent,recvbuf,sizeof(sent)))goto done;
    stage=0;
done:
    if(accepted>=0)close(accepted);
    if(client>=0)close(client);
    if(server>=0)close(server);
    socketExit();
    return stage;
}
#endif
