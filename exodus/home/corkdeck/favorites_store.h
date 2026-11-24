/* Local EXODUS preferences only. No game or Nintendo storage mutation. */
#ifndef EXODUS_FAVORITES_STORE_H
#define EXODUS_FAVORITES_STORE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#define EXODUS_FAV_MAX 4096
#define EXODUS_FAV_KEY 1024
struct exodus_favorites { char **keys; size_t count; };
static inline void exodus_fav_free(struct exodus_favorites *s) {
    for(size_t i=0;i<s->count;++i) free(s->keys[i]);
    free(s->keys); s->keys=NULL; s->count=0;
}
static inline size_t exodus_fav_find(const struct exodus_favorites *s,const char *key) {
    for(size_t i=0;i<s->count;++i) if(!strcmp(s->keys[i],key)) return i;
    return s->count;
}
static inline int exodus_fav_set(struct exodus_favorites *s,const char *key,int value) {
    size_t n=strlen(key); if(!n||n>=EXODUS_FAV_KEY) return 0;
    for(size_t i=0;i<n;++i) if((unsigned char)key[i]<32 || key[i]==127) return 0;
    size_t i=exodus_fav_find(s,key);
    if(i<s->count) {
        if(!value) { free(s->keys[i]); s->keys[i]=s->keys[--s->count]; }
        return 1;
    }
    if(!value) return 1;
    if(s->count>=EXODUS_FAV_MAX) return 0;
    char *copy=(char *)malloc(n+1); if(!copy) return 0; memcpy(copy,key,n+1);
    char **next=(char **)realloc(s->keys,(s->count+1)*sizeof(*next));
    if(!next) {free(copy);return 0;} s->keys=next; s->keys[s->count++]=copy; return 1;
}
/* Missing file is an empty store. Invalid/incomplete input never replaces state. */
static inline int exodus_fav_load(const char *path,struct exodus_favorites *s) {
    FILE *f=fopen(path,"rb"); if(!f) return errno==ENOENT;
    struct exodus_favorites next={NULL,0}; char line[EXODUS_FAV_KEY]; size_t n=0; int ch,ok=1;
    while((ch=fgetc(f))!=EOF) {
        if(ch=='\n') {
            line[n]=0;
            if(!n || exodus_fav_find(&next,line)!=next.count || !exodus_fav_set(&next,line,1)) {ok=0;break;}
            n=0;
        } else if(ch<32 || ch==127 || n>=sizeof(line)-1) {ok=0;break;}
        else line[n++]=(char)ch;
    }
    if(n || ferror(f)) ok=0;
    if(fclose(f)) ok=0;
    if(!ok) {exodus_fav_free(&next);return 0;}
    exodus_fav_free(s); *s=next; return 1;
}
/* Same-directory temporary file, flushed before rename; never unlink old store.
 * Assumes one shell writer and an owner-controlled preferences directory. */
static inline int exodus_fav_save(const char *path,const struct exodus_favorites *s) {
    char tmp[512]; int n=snprintf(tmp,sizeof(tmp),"%s.tmp.XXXXXX",path);
    if(n<0 || (size_t)n>=sizeof(tmp)) return 0;
    int fd=mkstemp(tmp); if(fd<0) return 0;
    FILE *f=fdopen(fd,"wb"); if(!f) {close(fd);unlink(tmp);return 0;}
    int ok=1;
    for(size_t i=0;i<s->count;++i) if(fprintf(f,"%s\n",s->keys[i])<0) {ok=0;break;}
    if(fflush(f) || fsync(fd)) ok=0;
    if(fclose(f)) ok=0;
    if(ok && rename(tmp,path)==0) return 1;
    unlink(tmp); return 0;
}
#endif
