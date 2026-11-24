#include "nro_artwork.h"
#include <assert.h>
static void put32(unsigned char *p, uint32_t value)
{
    for (unsigned i=0;i<4;i++) p[i]=(unsigned char)(value>>(8*i));
}
int main(void)
{
    unsigned char aset[56]={0};
    memcpy(aset,"ASET",4);
    put32(aset+8,56);
    put32(aset+16,16);
    uint64_t off=0;
    size_t len=0;
    assert(exodus_nro_icon_range(aset,72,&off,&len) && off==56 && len==16);
    assert(!exodus_nro_icon_range(aset,71,&off,&len));
    assert(!exodus_nro_icon_range(aset,55,&off,&len));
    put32(aset+4,1);
    assert(!exodus_nro_icon_range(aset,72,&off,&len));
    put32(aset+4,0);
    put32(aset+8,55);
    assert(!exodus_nro_icon_range(aset,72,&off,&len));
    put32(aset+8,56);
    put32(aset+12,1);
    assert(!exodus_nro_icon_range(aset,72,&off,&len));
    put32(aset+12,0);
    put32(aset+16,EXODUS_NRO_ICON_LIMIT+1);
    assert(!exodus_nro_icon_range(aset,UINT64_MAX,&off,&len));
    put32(aset+16,16);
    put32(aset+20,1);
    assert(!exodus_nro_icon_range(aset,UINT64_MAX,&off,&len));
    put32(aset+20,0);
    aset[0]='X';
    assert(!exodus_nro_icon_range(aset,72,&off,&len));
    return 0;
}
