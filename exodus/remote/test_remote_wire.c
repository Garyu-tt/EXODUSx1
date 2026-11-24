#include "remote_wire.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static exr_result read_mock(void *ctx,exr_op op,char *out,size_t cap){(void)ctx;(void)op;++calls;snprintf(out,cap,"ok");return EXR_OK;}
static size_t frame(unsigned char *b,const char *op){size_t n=strlen(op);memcpy(b,"EX\1\0",4);b[4]=0;b[5]=(unsigned char)n;b[6]=b[7]=0;memcpy(b+8,op,n);return 8+n;}
int main(void){
 exr_session s={1,1,0,10000,EXR_KNOWN};exr_provider p={EXR_KNOWN,EXR_KNOWN,read_mock,NULL};
 exw_conn c={0};unsigned char in[80],out[EXW_REPLY_MAX];size_t used,n,k=frame(in,"system.status");
 #define FEED(t,b,l) exw_feed(&c,&s,&p,t,b,l,&used,out,&n)
 for(size_t i=0;i<k;++i){assert(FEED(10,in+i,1)==(i+1==k?EXW_REPLY:EXW_MORE));assert(used==1);}
 assert(n==10&&out[3]==EXR_OK&&!memcmp(out+8,"ok",2)&&calls==1);
 memcpy(in+k,in,k);assert(FEED(11,in,2*k)==EXW_REPLY&&used==k);assert(FEED(11,in+k,k)==EXW_REPLY&&used==k);
 s.grants=EXR_BIT(EXR_DISCOVER);assert(FEED(12,in,k)==EXW_REPLY&&out[3]==EXR_DENIED&&n==8&&calls==3);
 s.grants=EXR_KNOWN;s.revoked=1;assert(FEED(13,in,k)==EXW_CLOSE&&used==0&&n==0);s.revoked=0;
 c=(exw_conn){0};assert(FEED(20,in,3)==EXW_MORE);assert(exw_tick(&c,&s,3020)==EXW_CLOSE);
 c=(exw_conn){0};in[2]=2;assert(FEED(20,in,k)==EXW_CLOSE&&calls==3);in[2]=1;
 c=(exw_conn){0};in[5]=33;assert(FEED(20,in,k)==EXW_CLOSE);in[5]=(unsigned char)(k-8);
 c=(exw_conn){0};for(unsigned i=0;i<EXW_LIMIT;++i)assert(FEED(30,in,k)==EXW_REPLY);assert(FEED(30,in,k)==EXW_CLOSE);
 c=(exw_conn){0};for(unsigned i=0;i<EXW_LIMIT;++i)assert(FEED(40,in,k)==EXW_REPLY);assert(FEED(1040,in,k)==EXW_REPLY);
 assert(exw_tick(&c,&s,1039)==EXW_CLOSE);
 c=(exw_conn){0};assert(FEED(9999,in,1)==EXW_MORE);assert(exw_tick(&c,&s,10000)==EXW_CLOSE);
 c=(exw_conn){0};s.authenticated=0;assert(FEED(1,in,k)==EXW_CLOSE);s.authenticated=1;
 c=(exw_conn){0};k=frame(in,"shell.exec");assert(FEED(1,in,k)==EXW_REPLY&&out[3]==EXR_BAD_REQUEST&&n==8);
 puts("EXR/1 framing, session and limits: passed (mock provider)");
}
