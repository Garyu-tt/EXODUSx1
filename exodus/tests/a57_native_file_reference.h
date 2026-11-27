/* Native FreeBSD file reference: one disposable private directory, one FD.
 * Fixed wire fields, no host paths, no arbitrary syscall dispatch. */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
enum { F_SIZE=64, F_OPEN=5,F_READ=3,F_WRITE=4,F_CLOSE=6 };
static int ffd=-1;
static unsigned fseq;
static int fcreated;
static char fdir[]="/var/tmp/exodus-fio.XXXXXX";
static char fpath[128];
static void fcleanup(void){
    if(ffd>=0)close(ffd);
    if(fpath[0])unlink(fpath);
    rmdir(fdir);
}
static int fio(const uint8_t q[64],uint8_t r[64]){
    if(memcmp(q,"AFI1",4)||q[4]!=1||q[5]!=1||q[7]||q[8]!=fseq+1)return 0;
    for(unsigned i=10;i<32;i++)if(q[i])return 0;
    unsigned op=q[6],n=q[9];
    if(n>16)return 0;
    for(unsigned i=32+(op==F_WRITE?n:0);i<64;i++)if(q[i])return 0;
    if((op==F_OPEN||op==F_CLOSE)&&n)return 0;
    if((op==F_READ||op==F_WRITE)&&!n)return 0;
    if(op!=F_OPEN&&op!=F_CLOSE&&op!=F_READ&&op!=F_WRITE)return 0;
    memcpy(r,q,64);r[5]=2;memset(r+32,0,32);
    ssize_t result=-1;
    if(op==F_OPEN){
        if(ffd>=0){errno=EBUSY;}
        else {
            if(!fpath[0]){
                if(!mkdtemp(fdir))return 0;
                int count=snprintf(fpath,sizeof fpath,"%s/data",fdir);
                if(count<0||(size_t)count>=sizeof fpath||atexit(fcleanup)){
                    rmdir(fdir);fpath[0]=0;
                    memcpy(fdir,"/var/tmp/exodus-fio.XXXXXX",sizeof fdir);
                    return 0;
                }
            }
            ffd=fcreated?open(fpath,O_RDONLY|O_NOFOLLOW):
                open(fpath,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
            if(ffd>=0){fcreated=1;result=3;}
        }
    }else if(ffd<0)errno=EBADF;
    else if(op==F_WRITE)result=write(ffd,q+32,n);
    else if(op==F_READ)result=read(ffd,r+32,n);
    else if(op==F_CLOSE){result=close(ffd);ffd=-1;}
    if(result<0){r[7]=(uint8_t)errno;r[10]=0;}
    else r[10]=(uint8_t)result;
    ++fseq;
    return 1;
}
