#define _POSIX_C_SOURCE 200809L
#include "library_state.h"
#include <assert.h>
int main(void)
{
    char directory[]="/tmp/exodus-state-XXXXXX";
    assert(mkdtemp(directory));
    char a[128],b[128];
    snprintf(a,sizeof(a),"%s/a",directory); snprintf(b,sizeof(b),"%s/b",directory);
    struct exodus_state s={0};
    exodus_state_open(&s,a,b);
    assert(s.writable && s.slot==-1);
    struct exodus_state_row *r=exodus_state_get(&s,"Switch","0000000000000001");
    assert(r); r->favorite=1; r->requested=7;
    assert(exodus_state_save(&s,a,b) && s.slot==0 && s.generation==1);
    r=exodus_state_get(&s,"Homebrew","app/app.nro");
    assert(r); r->requested=8;
    assert(exodus_state_save(&s,a,b) && s.slot==1 && s.generation==2);
    exodus_state_open(&s,a,b);
    assert(s.count==2 && s.generation==2);
    assert(exodus_state_find(&s,"Switch","0000000000000001")->favorite==1);
    assert(exodus_state_find(&s,"Homebrew","app/app.nro")->requested==8);
    assert(!exodus_state_find(&s,"Steam","app/app.nro"));
    FILE *f=fopen(b,"wb"); assert(f); assert(fputs("torn",f)>=0); assert(!fclose(f));
    exodus_state_open(&s,a,b);
    assert(s.count==1 && s.generation==1 && s.slot==0 && s.writable);
    assert(exodus_state_save(&s,a,b));
    f=fopen(a,"wb"); assert(f); assert(!fclose(f));
    f=fopen(b,"r+b"); assert(f); assert(!fseek(f,20,SEEK_SET)); assert(fputc(0xff,f)!=EOF); assert(!fclose(f));
    exodus_state_open(&s,a,b);
    assert(!s.writable && s.count==0 && !exodus_state_save(&s,a,b));
    exodus_state_clear(&s);
    assert(!unlink(a)); assert(!unlink(b)); assert(!rmdir(directory));
    return 0;
}
