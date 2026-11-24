#define _POSIX_C_SOURCE 200809L
#include "favorites_store.h"
#include <assert.h>
int main(void) {
    char root[]="/tmp/exodus-fav.XXXXXX";assert(mkdtemp(root));char path[512];snprintf(path,sizeof(path),"%s/favorites",root);
    struct exodus_favorites a={0},b={0};
    assert(exodus_fav_load(path,&a)&&!a.count);
    assert(exodus_fav_set(&a,"evcs-declared:0000000000000010",1));
    assert(exodus_fav_set(&a,"Steam:42",1));assert(exodus_fav_set(&a,"Steam:42",1)&&a.count==2);
    assert(!exodus_fav_set(&a,"bad\nkey",1));
    assert(exodus_fav_save(path,&a)&&exodus_fav_load(path,&b)&&b.count==2);
    assert(exodus_fav_find(&b,"evcs-declared:0000000000000010")<b.count);
    assert(exodus_fav_set(&b,"Steam:42",0)&&b.count==1);assert(exodus_fav_save(path,&b));
    assert(exodus_fav_load(path,&a)&&a.count==1);
    FILE *f=fopen(path,"wb");assert(f);fputs("broken",f);assert(!fclose(f));
    assert(!exodus_fav_load(path,&a)&&a.count==1);
    f=fopen(path,"wb");assert(f);fputs("Steam:42\nSteam:42\n",f);assert(!fclose(f));
    assert(!exodus_fav_load(path,&a)&&a.count==1);
    assert(exodus_fav_save(path,&a));
    char bad[512];snprintf(bad,sizeof(bad),"%s/missing/favorites",root);assert(!exodus_fav_save(bad,&a));
    assert(exodus_fav_load(path,&b)&&b.count==1);
    exodus_fav_free(&a);exodus_fav_free(&b);assert(!unlink(path));assert(!rmdir(root));puts("PASS: favorites persistence, dedup, removal, invalid input rollback, failed save");
}
