/* Bounded metadata-only inventory of FAT32 split archive directories.
 * Recognizes 00..99, contiguous, equally sized nonfinal parts; no joining,
 * payload reads, rights changes or inference that the last part is complete. */
#ifndef EXODUS_EVCS_SPLIT_H
#define EXODUS_EVCS_SPLIT_H
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum evcs_split_result { EVCS_SPLIT_OK, EVCS_SPLIT_INVALID, EVCS_SPLIT_IO, EVCS_SPLIT_LIMIT };
struct evcs_split_info { unsigned parts; uint64_t bytes; };
static inline enum evcs_split_result evcs_split_probe(const char *root, unsigned *visited,
                                                      unsigned visit_limit, struct evcs_split_info *out)
{
    if (!out || !visited || !root) return EVCS_SPLIT_INVALID;
    out->parts=0; out->bytes=0;
    struct stat st;
    if (lstat(root,&st)) return EVCS_SPLIT_IO;
    if (!S_ISDIR(st.st_mode)) return EVCS_SPLIT_INVALID;
    DIR *d=opendir(root);
    if (!d) return EVCS_SPLIT_IO;
    uint64_t sizes[100]={0};
    unsigned parts=0;
    enum evcs_split_result rc=EVCS_SPLIT_OK;
    for (;;) {
        errno=0;
        struct dirent *e=readdir(d);
        if (!e) { if (errno) rc=EVCS_SPLIT_IO; break; }
        const char *name=e->d_name;
        if (!strcmp(name,".") || !strcmp(name,"..")) continue;
        if (*visited>=visit_limit) { rc=EVCS_SPLIT_LIMIT; break; }
        ++*visited;
        if (strlen(name)!=2 || name[0]<'0' || name[0]>'9' || name[1]<'0' || name[1]>'9') {
            rc=EVCS_SPLIT_INVALID; break;
        }
        unsigned index=(unsigned)(name[0]-'0')*10u+(unsigned)(name[1]-'0');
        char path[512];
        int n=snprintf(path,sizeof(path),"%s/%s",root,name);
        if (n<0 || (size_t)n>=sizeof(path)) { rc=EVCS_SPLIT_LIMIT; break; }
        if (lstat(path,&st)) { rc=EVCS_SPLIT_IO; break; }
        if (!S_ISREG(st.st_mode) || st.st_size<=0 || (uint64_t)st.st_size>UINT32_MAX || sizes[index]) {
            rc=EVCS_SPLIT_INVALID; break;
        }
        sizes[index]=(uint64_t)st.st_size;
        ++parts;
    }
    if (closedir(d) && rc==EVCS_SPLIT_OK) rc=EVCS_SPLIT_IO;
    if (rc!=EVCS_SPLIT_OK) return rc;
    if (parts<2) return EVCS_SPLIT_INVALID;
    uint64_t total=0;
    for (unsigned i=0;i<parts;++i) {
        if (!sizes[i] || (i+1<parts && sizes[i]!=sizes[0]) || sizes[i]>sizes[0])
            return EVCS_SPLIT_INVALID;
        total+=sizes[i]; /* at most 100 * UINT32_MAX */
    }
    out->parts=parts; out->bytes=total;
    return EVCS_SPLIT_OK;
}
#endif
