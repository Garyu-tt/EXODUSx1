/* SPDX-License-Identifier: GPL-2.0-only
 * Explicit owner-supplied sidecar, never evidence of entitlement.
 * Grammar: EVCS1 <16hex app> <16hex title> <decimal version> <base|update|dlc>
 * Single line, optional final LF or CRLF. */
#ifndef EXODUS_EVCS_MANIFEST_H
#define EXODUS_EVCS_MANIFEST_H
#include "evcs_graph.h"
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

enum evcs_manifest_result { EVCS_MANIFEST_OK, EVCS_MANIFEST_MISSING, EVCS_MANIFEST_INVALID, EVCS_MANIFEST_IO };
static inline int evcs_manifest_hex(const char **p, uint64_t *out)
{
    uint64_t n=0;
    for (unsigned i=0;i<16;++i) {
        unsigned v; char c=**p;
        if (c>='0' && c<='9') v=(unsigned)(c-'0');
        else if (c>='a' && c<='f') v=(unsigned)(c-'a'+10);
        else if (c>='A' && c<='F') v=(unsigned)(c-'A'+10);
        else return 0;
        n=(n<<4)|v; ++*p;
    }
    if (**p!=' ' || !n) return 0;
    ++*p; *out=n; return 1;
}
static inline int evcs_manifest_parse(const char *s, struct evcs_content *out)
{
    if (!s || !out || strncmp(s,"EVCS1 ",6)) return 0;
    const char *p=s+6;
    struct evcs_content x={0,0,0,EVCS_BASE};
    if (!evcs_manifest_hex(&p,&x.application_id) ||
        !evcs_manifest_hex(&p,&x.content_id)) return 0;
    unsigned digits=0;
    while (*p>='0' && *p<='9') {
        unsigned v=(unsigned)(*p-'0');
        if (++digits>10 || x.version>(UINT32_MAX-v)/10u) return 0;
        x.version=x.version*10u+v; ++p;
    }
    if (!digits || *p++!=' ') return 0;
    if (!strcmp(p,"base")) x.role=EVCS_BASE;
    else if (!strcmp(p,"update")) x.role=EVCS_UPDATE;
    else if (!strcmp(p,"dlc")) x.role=EVCS_DLC;
    else return 0;
    if (evcs_graph_validate(&x,1)!=EVCS_GRAPH_OK) return 0;
    *out=x; return 1;
}
static inline enum evcs_manifest_result evcs_manifest_read(const char *archive, struct evcs_content *out)
{
    char path[520], buf[80];
    if (!archive || !out) return EVCS_MANIFEST_INVALID;
    int len=snprintf(path,sizeof(path),"%s.evcs",archive);
    if (len<0 || (size_t)len>=sizeof(path)) return EVCS_MANIFEST_INVALID;
    struct stat st;
    if (lstat(path,&st)) return errno==ENOENT ? EVCS_MANIFEST_MISSING : EVCS_MANIFEST_IO;
    if (!S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>=(off_t)sizeof(buf))
        return EVCS_MANIFEST_INVALID;
    FILE *f=fopen(path,"rb");
    if (!f) return EVCS_MANIFEST_IO;
    size_t n=fread(buf,1,sizeof(buf),f);
    int err=ferror(f);
    if (fclose(f) || err) return EVCS_MANIFEST_IO;
    if (n!=(size_t)st.st_size || n>=sizeof(buf) || memchr(buf,0,n)) return EVCS_MANIFEST_INVALID;
    if (n && buf[n-1]=='\n') { --n; if (n && buf[n-1]=='\r') --n; }
    buf[n]=0;
    return evcs_manifest_parse(buf,out) ? EVCS_MANIFEST_OK : EVCS_MANIFEST_INVALID;
}
#endif
