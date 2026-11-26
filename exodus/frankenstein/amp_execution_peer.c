/* Reference process transport: stdio connected by a host harness.
 * No shared hardware mapping or actual FreeBSD syscalls. */
#include "amp_execution_contract.c"
#include <stdio.h>
enum { COUNT=128 };
static const uint64_t gen=UINT64_C(0x1020304050607080),task=UINT64_C(0x1234567887654321);
static int emit(const uint8_t *b){return fwrite(b,1,AMP_SIZE,stdout)==AMP_SIZE&&fflush(stdout)==0;}
static int read_frame(uint8_t *b){return fread(b,1,AMP_SIZE,stdin)==AMP_SIZE;}
int main(int argc,char **argv){
    struct amp_session s;uint8_t q[AMP_SIZE],r[AMP_SIZE];uint64_t result;
    if(argc!=2||!init(&s,gen,task))return 1;
    s.last=UINT64_C(0xffffffc0);
    if(!strcmp(argv[1],"authority")){
        for(unsigned i=0;i<COUNT+1;i++){
            if(!read_frame(q)||!serve(&s,q,sizeof q,r)||!emit(r))return 2;
            if(s.closed!=(i==COUNT))return 3;
        }
    }else if(!strcmp(argv[1],"executor")){
        for(unsigned i=0;i<COUNT+1;i++){
            uint8_t op=i==COUNT?AMP_EXIT_INTENT:AMP_GETPID;
            if(!request(&s,op,i==COUNT?23:0,q)||!emit(q)||!read_frame(r)||
               !complete(&s,r,sizeof r,&result)||result!=(i==COUNT?0:task))return 4;
        }
    }else return 1;
    if(!s.closed||s.last!=UINT64_C(0x100000041))return 5;
    fprintf(stderr,"AMP_PEER_PASS %s width=%zu seq=%llu\n",argv[1],sizeof(void*)*8,(unsigned long long)s.last);
    return 0;
}
