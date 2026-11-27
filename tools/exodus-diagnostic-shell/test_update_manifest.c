#include "update_manifest.h"
#include <assert.h>
int main(void){
    const char *hex="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char line[180],manifest[128];TdaUpdate u;
    snprintf(line,sizeof line,"update 10 4096 %s %s",hex,hex);
    assert(td_parse(line,&u)&&u.rev==10&&u.size==4096);
    assert(td_manifest(&u,manifest,sizeof manifest)>0);
    assert(!strncmp(manifest,"EXODUS-NRO-UPDATE/1\n10\n4096\n",27));
    snprintf(line,sizeof line,"update 9 4096 %s %s",hex,hex);assert(!td_parse(line,&u));
    snprintf(line,sizeof line,"update 10 4194305 %s %s",hex,hex);assert(!td_parse(line,&u));
    snprintf(line,sizeof line,"update 010 4096 %s %s",hex,hex);assert(!td_parse(line,&u));
    snprintf(line,sizeof line,"update 10 4096 %s %s x",hex,hex);assert(!td_parse(line,&u));
    unsigned char b[128]={0};memcpy(b+16,"NRO0",4);b[25]=1;
    assert(td_nro_header(b,128,4096));assert(!td_nro_header(b,128,128));
    unsigned char x[32],y[32];assert(td_hex(hex,x)&&td_hex(hex,y)&&td_equal(x,y,32));y[31]^=1;assert(!td_equal(x,y,32));
    puts("manifest, revision, length, canonical encoding, NRO header and digest checks PASS");
}
