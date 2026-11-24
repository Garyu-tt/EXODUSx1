#ifndef EXODUS_TCP_MTLS_BIO_H
#define EXODUS_TCP_MTLS_BIO_H
/* Diagnostic transport only: no external bind, no remote operations. */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/platform_time.h>
typedef struct {int client,server,initialized;} ext_pair;
static void ext_close(ext_pair *p){
    if(p->client>=0)close(p->client);
    if(p->server>=0)close(p->server);
#ifdef __SWITCH__
    if(p->initialized)socketExit();
#endif
    p->client=p->server=-1;p->initialized=0;
}
static int ext_nonblock(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    return flags>=0&&fcntl(fd,F_SETFL,flags|O_NONBLOCK)==0;
}
static int ext_ready(int fd,short events){
    struct pollfd p={.fd=fd,.events=events};
    return poll(&p,1,1000)==1&&(p.revents&events)&&!(p.revents&(POLLERR|POLLHUP|POLLNVAL));
}
static int ext_open(ext_pair *p){
    int listener=-1,ok=0;
#ifdef __SWITCH__
    if(R_FAILED(socketInitializeDefault()))return 0;
    p->initialized=1;
#endif
    struct sockaddr_in a={0},peer={0};socklen_t len=sizeof(a),plen=sizeof(peer);
    a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener<0||!ext_nonblock(listener)||bind(listener,(struct sockaddr*)&a,sizeof(a))||
       listen(listener,1)||getsockname(listener,(struct sockaddr*)&a,&len)||
       a.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||!a.sin_port)goto done;
    p->client=socket(AF_INET,SOCK_STREAM,0);
    if(p->client<0||!ext_nonblock(p->client))goto done;
    int rc=connect(p->client,(struct sockaddr*)&a,sizeof(a));
    if(rc<0&&errno!=EINPROGRESS&&errno!=EWOULDBLOCK)goto done;
    if(!ext_ready(listener,POLLIN))goto done;
    p->server=accept(listener,(struct sockaddr*)&peer,&plen);
    if(p->server<0||!ext_nonblock(p->server)||peer.sin_family!=AF_INET||
       peer.sin_addr.s_addr!=htonl(INADDR_LOOPBACK))goto done;
    int err=0;socklen_t elen=sizeof(err);
    if(!ext_ready(p->client,POLLOUT)||getsockopt(p->client,SOL_SOCKET,SO_ERROR,&err,&elen)||err)goto done;
    /* Accept only the exact local client created by this diagnostic. */
    struct sockaddr_in local={0},remote={0};
    socklen_t llen=sizeof(local),rlen=sizeof(remote);
    if(getsockname(p->client,(struct sockaddr*)&local,&llen)||
       getpeername(p->server,(struct sockaddr*)&remote,&rlen)||
       local.sin_family!=AF_INET||remote.sin_family!=AF_INET||
       local.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||
       local.sin_addr.s_addr!=peer.sin_addr.s_addr||
       local.sin_port!=peer.sin_port||
       remote.sin_addr.s_addr!=local.sin_addr.s_addr||
       remote.sin_port!=local.sin_port)goto done;
    ok=1;
done:
    if(listener>=0)close(listener);
    if(!ok)ext_close(p);
    return ok;
}
static int ext_send(void *ctx,const unsigned char *b,size_t n){
    if(n>16384)n=16384;
#ifdef MSG_NOSIGNAL
    ssize_t r=send(*(int*)ctx,b,n,MSG_NOSIGNAL);
#else
    ssize_t r=send(*(int*)ctx,b,n,0);
#endif
    if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?
        MBEDTLS_ERR_SSL_WANT_WRITE:MBEDTLS_ERR_NET_SEND_FAILED;
    return (int)r;
}
static int ext_recv(void *ctx,unsigned char *b,size_t n){
    if(n>16384)n=16384;
    ssize_t r=recv(*(int*)ctx,b,n,0);
    if(r<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?
        MBEDTLS_ERR_SSL_WANT_READ:MBEDTLS_ERR_NET_RECV_FAILED;
    return (int)r;
}
static void ext_wait(const ext_pair *p){
    struct pollfd f[2]={{.fd=p->client,.events=POLLIN},{.fd=p->server,.events=POLLIN}};
    (void)poll(f,2,10);
}
static int ext_within(mbedtls_ms_time_t start){
    mbedtls_ms_time_t now=mbedtls_ms_time();
    return now>=start && now-start<5000;
}
#endif
