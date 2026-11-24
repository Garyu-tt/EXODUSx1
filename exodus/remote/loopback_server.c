#define _POSIX_C_SOURCE 200809L
#include "remote_session.h"
#include <openssl/pem.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define HANDSHAKE_MS UINT64_C(5000)
#define SESSION_MS UINT64_C(60000)
static volatile sig_atomic_t stopping;
static void stop(int sig){(void)sig;stopping=1;}
static int now_ms(uint64_t *v){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;*v=(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;return 1;}
static int nonblock(int fd){int f=fcntl(fd,F_GETFL,0);return f>=0&&fcntl(fd,F_SETFL,f|O_NONBLOCK)==0;}
static int wait_fd(int fd,short events,int timeout){struct pollfd p={fd,events,0};int r=poll(&p,1,timeout);if(r<0&&errno==EINTR)return 0;if(r<0||(r&&p.revents&(POLLERR|POLLHUP|POLLNVAL)))return -1;return r;}
static int load_peer(const char *path,ex_tls_peer *peer){
 FILE *f=fopen(path,"r");if(!f)return 0;X509 *cert=PEM_read_X509(f,NULL,NULL,NULL);fclose(f);if(!cert)return 0;
 unsigned n=0;int ok=X509_digest(cert,EVP_sha256(),peer->pin,&n);X509_free(cert);
 peer->enabled=1;peer->grants=EXR_BIT(EXR_DISCOVER)|EXR_BIT(EXR_STATUS);return ok==1&&n==32;
}
static exr_result host_read(void *ctx,exr_op op,char *out,size_t cap){(void)ctx;if(op!=EXR_STATUS)return EXR_UNAVAILABLE;int n=snprintf(out,cap,"scope=host_reference\nhardware_verified=false\n");return n>=0&&(size_t)n<cap?EXR_OK:EXR_PROVIDER_ERROR;}
static void serve(int fd,SSL_CTX *ctx,const ex_tls_peer *peer){
 SSL *tls=SSL_new(ctx);uint64_t begin,now;exs_conn s={0};int ready=0;
 if(!tls||!SSL_set_fd(tls,fd)||!now_ms(&begin))goto done;
 SSL_set_accept_state(tls);
 while(!stopping&&now_ms(&now)&&now>=begin&&now-begin<HANDSHAKE_MS){
  ERR_clear_error();int rc=SSL_accept(tls);if(rc==1){ready=1;break;}
  int e=SSL_get_error(tls,rc);if(e!=SSL_ERROR_WANT_READ&&e!=SSL_ERROR_WANT_WRITE)break;
  if(wait_fd(fd,e==SSL_ERROR_WANT_READ?POLLIN:POLLOUT,100)<0)break;
 }
 if(!ready||stopping||!now_ms(&now)||!exs_init(&s,tls,now,now+SESSION_MS,1,peer,1))goto done;
 const exr_provider p={EXR_BIT(EXR_DISCOVER)|EXR_BIT(EXR_STATUS),EXR_BIT(EXR_DISCOVER)|EXR_BIT(EXR_STATUS),host_read,NULL};
 while(!stopping&&now_ms(&now)){
  exs_state r=exs_step(&s,now,1,peer,1,&p);if(r==EXS_CLOSE)break;
  if(r==EXS_PROGRESS)continue;
  if(wait_fd(fd,r==EXS_WANT_READ?POLLIN:POLLOUT,100)<0)break;
 }
 done: OPENSSL_cleanse(&s,sizeof(s));SSL_free(tls);close(fd);
}
/* Deliberately host-only and loopback-only. One active connection, no fork,
 * no shell, no file operations. Local owner explicitly starts foreground. */
int main(int argc,char **argv){
 if(argc!=5){fprintf(stderr,"usage: %s CA SERVER_CERT SERVER_KEY PAIRED_CLIENT_CERT\n",argv[0]);return 2;}
 struct sigaction sa={0};sa.sa_handler=stop;sigemptyset(&sa.sa_mask);if(sigaction(SIGINT,&sa,NULL)||sigaction(SIGTERM,&sa,NULL))return 1;
 sa.sa_handler=SIG_IGN;if(sigaction(SIGPIPE,&sa,NULL))return 1;
 ex_tls_peer peer={0};SSL_CTX *ctx=ex_tls_server_ctx(argv[1],argv[2],argv[3]);int listener=-1,status=1;
 if(!ctx||!load_peer(argv[4],&peer))goto done;
 listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0||!nonblock(listener))goto done;
 struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=0;
 if(bind(listener,(struct sockaddr *)&addr,sizeof(addr))||listen(listener,1))goto done;
 socklen_t len=sizeof(addr);if(getsockname(listener,(struct sockaddr *)&addr,&len))goto done;
 printf("READY %u\n",(unsigned)ntohs(addr.sin_port));fflush(stdout);
 while(!stopping){int r=wait_fd(listener,POLLIN,100);if(r<0)goto done;if(!r)continue;
  int fd=accept(listener,NULL,NULL);if(fd<0){if(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)continue;goto done;}
  if(!nonblock(fd)){close(fd);continue;}serve(fd,ctx,&peer);
 }
 status=0;
 done: if(listener>=0)close(listener);SSL_CTX_free(ctx);OPENSSL_cleanse(&peer,sizeof(peer));return status;
}
