/* EVCS R01: bounded, read-only folder inventory; no launch or import. */
#ifndef EXODUS_EVCS_DISCOVERY_H
#define EXODUS_EVCS_DISCOVERY_H
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include "evcs_split.h"

#define EVCS_PATH_CAP 512
#define EVCS_MAX_DEPTH 8
#define EVCS_MAX_VISITED 16384
#define EVCS_MAX_FILES 4096

enum evcs_kind { EVCS_NRO, EVCS_NSP, EVCS_XCI, EVCS_NCA };
enum evcs_result { EVCS_OK, EVCS_LIMIT, EVCS_IO, EVCS_STOP };
/* Callback paths are borrowed for this call only. Every item is discovered,
 * never verified or launch-ready. The caller must discard partial catalogs. */
typedef int (*evcs_emit)(const char *, enum evcs_kind, uint64_t, void *);
struct evcs_scan { unsigned visited, files; evcs_emit emit; void *user; };

static inline int evcs_kind_of(const char *name, enum evcs_kind *out)
{
    const char *p = strrchr(name, '.');
    char ext[4];
    if (!p || p == name || strlen(p) != 4) return 0;
    for (unsigned i = 0; i < 3; ++i) {
        char c = p[i+1];
        ext[i] = c >= 'A' && c <= 'Z' ? (char)(c + ('a'-'A')) : c;
    }
    ext[3] = 0;
    if (!strcmp(ext,"nro")) *out = EVCS_NRO;
    else if (!strcmp(ext,"nsp")) *out = EVCS_NSP;
    else if (!strcmp(ext,"xci")) *out = EVCS_XCI;
    else if (!strcmp(ext,"nca")) *out = EVCS_NCA;
    else return 0;
    return 1;
}
static inline int evcs_safe_name(const char *s)
{
    if (!*s || !strcmp(s,".") || !strcmp(s,"..")) return 0;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c < 32 || c == 127 || c == '/' || c == 92 || c == ':' || c == '"') return 0;
    }
    return 1;
}
/* POSIX reference walker. No follow of static symlinks. Requires a stable,
 * owner-controlled root: not a sandbox for adversarial concurrent mutation. */
static inline enum evcs_result evcs_walk(const char *root, unsigned depth, struct evcs_scan *s)
{
    DIR *d = opendir(root);
    if (!d) return EVCS_IO;
    enum evcs_result rc = EVCS_OK;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(d);
        if (!e) { if (evcs_directory_read_failed(errno)) rc = EVCS_IO; break; }
        if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        if (++s->visited > EVCS_MAX_VISITED) { rc = EVCS_LIMIT; break; }
        if (!evcs_safe_name(e->d_name)) continue;
        char path[EVCS_PATH_CAP];
        int n = snprintf(path,sizeof(path),"%s/%s",root,e->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) { rc = EVCS_LIMIT; break; }
        struct stat st;
        if (lstat(path,&st)) { rc = EVCS_IO; break; }
        if (S_ISDIR(st.st_mode)) {
            enum evcs_kind kind;
            if (evcs_kind_of(e->d_name,&kind) && (kind==EVCS_NSP || kind==EVCS_XCI)) {
                struct evcs_split_info info;
                enum evcs_split_result split=evcs_split_probe(path,&s->visited,EVCS_MAX_VISITED,&info);
                if (split==EVCS_SPLIT_INVALID) continue;
                if (split!=EVCS_SPLIT_OK) { rc=split==EVCS_SPLIT_LIMIT ? EVCS_LIMIT : EVCS_IO; break; }
                if (++s->files > EVCS_MAX_FILES) { rc=EVCS_LIMIT; break; }
                if (!s->emit(path,kind,info.bytes,s->user)) { rc=EVCS_STOP; break; }
                continue;
            }
            if (depth >= EVCS_MAX_DEPTH) { rc = EVCS_LIMIT; break; }
            rc = evcs_walk(path,depth+1,s);
            if (rc != EVCS_OK) break;
        } else if (S_ISREG(st.st_mode) && st.st_size > 0) {
            enum evcs_kind k;
            if (!evcs_kind_of(e->d_name,&k)) continue;
            if (++s->files > EVCS_MAX_FILES) { rc = EVCS_LIMIT; break; }
            if (!s->emit(path,k,(uint64_t)st.st_size,s->user)) { rc = EVCS_STOP; break; }
        }
    }
    if (closedir(d) && rc == EVCS_OK) rc = EVCS_IO;
    return rc;
}
static inline enum evcs_result evcs_discover(const char *root, evcs_emit emit, void *user)
{
    struct stat st;
    if (!root || !*root || !emit || strlen(root) >= EVCS_PATH_CAP-1 ||
        root[strlen(root)-1] == '/' || lstat(root,&st) || !S_ISDIR(st.st_mode)) return EVCS_IO;
    struct evcs_scan s = {0,0,emit,user};
    return evcs_walk(root,0,&s);
}
#endif
