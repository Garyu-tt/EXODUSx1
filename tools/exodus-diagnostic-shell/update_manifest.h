/* EXODUS authenticated update manifest. No key provisioning over network. */
#ifndef TDA_UPDATE_H
#define TDA_UPDATE_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TDA_REV 9
#define TDA_UPDATE_MAX (4u*1024u*1024u)
typedef struct { unsigned rev, size; char digest[65], mac[65]; } TdaUpdate;
static inline int td_hex(const char *s,unsigned char out[32]){
    if(strlen(s)!=64)return 0;
    for(unsigned i=0;i<32;i++){
        unsigned v=0;
        for(unsigned j=0;j<2;j++){
            char c=s[2*i+j];unsigned d;
            if(c>='0'&&c<='9')d=(unsigned)(c-'0');
            else if(c>='a'&&c<='f')d=(unsigned)(c-'a'+10);
            else return 0;
            v=v*16+d;
        }
        out[i]=(unsigned char)v;
    }
    return 1;
}
static inline int td_equal(const unsigned char *a,const unsigned char *b,size_t n){
    unsigned diff=0;for(size_t i=0;i<n;i++)diff|=a[i]^b[i];return !diff;
}
static inline int td_manifest(const TdaUpdate *u,char *out,size_t cap){
    int n=snprintf(out,cap,"EXODUS-NRO-UPDATE/1\n%u\n%u\n%s\n",u->rev,u->size,u->digest);
    return n>0&&(size_t)n<cap?n:0;
}
static inline int td_parse(const char *line,TdaUpdate *u){
    char canonical[180],rev[5],size[8],extra;
    unsigned char raw[32];TdaUpdate v={0};
    if(sscanf(line,"update %4[0-9] %7[0-9] %64[0-9a-f] %64[0-9a-f]%c",rev,size,v.digest,v.mac,&extra)!=4)return 0;
    for(const char *p=rev;*p;p++)v.rev=v.rev*10+(unsigned)(*p-'0');
    for(const char *p=size;*p;p++)v.size=v.size*10+(unsigned)(*p-'0');
    if(v.rev<=TDA_REV||v.rev>9999||v.size<128||v.size>TDA_UPDATE_MAX||!td_hex(v.digest,raw)||!td_hex(v.mac,raw))return 0;
    int n=snprintf(canonical,sizeof canonical,"update %u %u %s %s",v.rev,v.size,v.digest,v.mac);
    if(n<0||(size_t)n>=sizeof canonical||strcmp(line,canonical))return 0;
    *u=v;return 1;
}
static inline int td_nro_header(const unsigned char *b,size_t n,unsigned size){
    if(n<128||memcmp(b+16,"NRO0",4))return 0;
    uint32_t image=(uint32_t)b[24]|((uint32_t)b[25]<<8)|((uint32_t)b[26]<<16)|((uint32_t)b[27]<<24);
    return image>=128&&image<=size;
}
#endif
