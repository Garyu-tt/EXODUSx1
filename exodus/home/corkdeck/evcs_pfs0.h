/* SPDX-License-Identifier: GPL-2.0-only
 * Read-only PFS0 directory probe. No payload reads, extraction or rights checks. */
#ifndef EXODUS_EVCS_PFS0_H
#define EXODUS_EVCS_PFS0_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVCS_PFS0_MAX_ENTRIES 1024u
#define EVCS_PFS0_MAX_STRINGS 65536u
enum evcs_pfs0_result { EVCS_PFS0_OK, EVCS_PFS0_INVALID, EVCS_PFS0_LIMIT, EVCS_PFS0_IO };
struct evcs_pfs0_info { uint32_t entries, nca, meta_nca; uint64_t directory_bytes; };
static inline uint32_t evcs_pfs0_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline uint64_t evcs_pfs0_u64(const unsigned char *p)
{
    return (uint64_t)evcs_pfs0_u32(p) | (uint64_t)evcs_pfs0_u32(p+4)<<32;
}
static inline int evcs_pfs0_suffix(const char *s, size_t n, const char *suffix)
{
    const size_t len = strlen(suffix);
    return n >= len && !memcmp(s+n-len,suffix,len);
}
static inline enum evcs_pfs0_result evcs_pfs0_probe(FILE *f, uint64_t file_bytes,
                                                    struct evcs_pfs0_info *out)
{
    if (!f || !out) return EVCS_PFS0_INVALID;
    memset(out,0,sizeof(*out));
    unsigned char header[16];
    if (file_bytes < sizeof(header)) return EVCS_PFS0_INVALID;
    if (fseek(f,0,SEEK_SET) || fread(header,1,sizeof(header),f)!=sizeof(header))
        return EVCS_PFS0_IO;
    if (memcmp(header,"PFS0",4)) return EVCS_PFS0_INVALID;
    uint32_t count=evcs_pfs0_u32(header+4), strings=evcs_pfs0_u32(header+8);
    if (count > EVCS_PFS0_MAX_ENTRIES || strings > EVCS_PFS0_MAX_STRINGS)
        return EVCS_PFS0_LIMIT;
    if (!count || !strings) return EVCS_PFS0_INVALID;
    size_t table=(size_t)count*24, bytes=table+strings;
    uint64_t data=16+(uint64_t)bytes;
    if (data > file_bytes) return EVCS_PFS0_INVALID;
    unsigned char *buf=(unsigned char *)malloc(bytes);
    if (!buf) return EVCS_PFS0_IO;
    enum evcs_pfs0_result rc=EVCS_PFS0_INVALID;
    struct evcs_pfs0_info info={count,0,0,data};
    if (fread(buf,1,bytes,f)!=bytes) { rc=EVCS_PFS0_IO; goto finish; }
    for (uint32_t i=0;i<count;++i) {
        const unsigned char *e=buf+24*i;
        uint64_t off=evcs_pfs0_u64(e), size=evcs_pfs0_u64(e+8);
        uint32_t name=evcs_pfs0_u32(e+16);
        if (off > file_bytes-data || size > file_bytes-data-off || name >= strings)
            goto finish;
        const char *s=(const char *)buf+table+name;
        const char *end=(const char *)memchr(s,0,strings-name);
        if (!end || end==s) goto finish;
        size_t len=(size_t)(end-s);
        if ((len==1 && s[0]=='.') || (len==2 && s[0]=='.' && s[1]=='.')) goto finish;
        for (size_t j=0;j<len;++j) {
            unsigned char ch=(unsigned char)s[j];
            if (ch<32 || ch==127 || ch=='/' || ch==92 || ch==':') goto finish;
        }
        for (uint32_t j=0;j<i;++j) {
            const unsigned char *prev=buf+24*j;
            uint32_t prevname=evcs_pfs0_u32(prev+16);
            uint64_t po=evcs_pfs0_u64(prev), ps=evcs_pfs0_u64(prev+8);
            if (!strcmp(s,(const char *)buf+table+prevname)) goto finish;
            if (size && ps && off<po+ps && po<off+size) goto finish;
        }
        if (evcs_pfs0_suffix(s,len,".nca")) ++info.nca;
        if (evcs_pfs0_suffix(s,len,".cnmt.nca")) ++info.meta_nca;
    }
    *out=info;
    rc=EVCS_PFS0_OK;
finish:
    free(buf);
    return rc;
}
#endif
