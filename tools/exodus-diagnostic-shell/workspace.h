/* SPDX-License-Identifier: MIT
 * EXODUS diagnostic workspace; isolated RAM files and bounded C-like statements. */
#ifndef TDA_WORKSPACE_H
#define TDA_WORKSPACE_H
#include "diag_vm.h"
#include <stdio.h>
#define WS_FILES 16
#define WS_SOURCE 768
typedef struct { char name[25], source[WS_SOURCE+1]; } WsFile;
typedef struct { WsFile files[WS_FILES]; } Workspace;
typedef int (*WsProbe)(const char *,uint64_t *);
typedef struct { uint64_t r[8], mem[256]; unsigned steps; } WsResult;
static inline int ws_name(const char *s){
    size_t n=strlen(s);
    if(!n||n>24)return 0;
    for(size_t i=0;i<n;i++){
        char c=s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;
    }
    return 1;
}
static inline WsFile *ws_find(Workspace *w,const char *name){
    if(!ws_name(name))return NULL;
    for(unsigned i=0;i<WS_FILES;i++)if(!strcmp(w->files[i].name,name))return &w->files[i];
    return NULL;
}
static inline const char *ws_put(Workspace *w,const char *name,const char *src){
    size_t n=strlen(src);
    if(!ws_name(name))return "name";
    if(!n||n>WS_SOURCE)return "length";
    for(size_t i=0;i<n;i++)if((unsigned char)src[i]<32||(unsigned char)src[i]>126)return "ascii";
    WsFile *f=ws_find(w,name);
    if(!f)for(unsigned i=0;i<WS_FILES;i++)if(!w->files[i].name[0]){f=&w->files[i];break;}
    if(!f)return "full";
    memcpy(f->name,name,strlen(name)+1);memcpy(f->source,src,n+1);
    return NULL;
}
static inline const char *ws_rm(Workspace *w,const char *name){
    WsFile *f=ws_find(w,name);if(!f)return "not_found";
    memset(f,0,sizeof *f);return NULL;
}
static inline void ws_space(const char **p){while(**p==' ')(*p)++;}
static inline int ws_reg(const char **p,unsigned *i){
    ws_space(p);
    if(**p!='r'||(*p)[1]<'0'||(*p)[1]>'7')return 0;
    *i=(unsigned)((*p)[1]-'0');*p+=2;return 1;
}
/* Addressing is exclusively within the per-run 256-word scratch array. */
static inline int ws_slot(const char **p,const WsResult *r,unsigned *i){
    ws_space(p);
    if((*p)[0]!='m'||(*p)[1]!='[')return 0;
    *p+=2;ws_space(p);
    uint64_t n;
    if(**p=='r'){
        unsigned k;if(!ws_reg(p,&k))return 0;n=r->r[k];
    }else{
        char buf[67];size_t len=0;
        while(**p&&**p!=']'&&**p!=' '){
            if(len==sizeof buf-1)return 0;
            buf[len++]=*(*p)++;
        }
        buf[len]=0;const char *q=buf;
        if(!tda_num(&q,&n)||*q)return 0;
    }
    ws_space(p);if(**p!=']'||n>=256)return 0;
    (*p)++;*i=(unsigned)n;return 1;
}
static inline int ws_value(const char **p,const WsResult *r,WsProbe probe,uint64_t *v){
    ws_space(p);unsigned i;
    if(**p=='r'){if(!ws_reg(p,&i))return 0;*v=r->r[i];return 1;}
    if((*p)[0]=='m'&&(*p)[1]=='['){if(!ws_slot(p,r,&i))return 0;*v=r->mem[i];return 1;}
    if(**p>='a'&&**p<='z'){
        char name[25];size_t n=0;
        while((**p>='a'&&**p<='z')||**p=='_'){
            if(n==sizeof name-1)return 0;
            name[n++]=*(*p)++;
        }
        name[n]=0;
        if(**p!='('||(*p)[1]!=')')return 0;
        *p+=2;return probe&&probe(name,v);
    }
    char num[67];size_t n=0;
    while(**p&&**p!=' '&&**p!=';'){
        if(n==sizeof num-1)return 0;
        num[n++]=*(*p)++;
    }
    num[n]=0;const char *q=num;
    return tda_num(&q,v)&&!*q;
}
/* repeat N { ... } with literal count 1..1024; max 4 nested blocks.
 * 4096 fuel units shared by all assignments and loop iterations.
 * Values are unsigned 64-bit and memory is reset each execution. */
static inline const char *ws_block(const char **p,WsProbe probe,WsResult *r,unsigned depth,unsigned *fuel){
    ws_space(p);
    while(**p&&**p!='}'){
        if(!*fuel)return "step_limit";
        --*fuel;
        if(!strncmp(*p,"repeat ",7)){
            if(depth>=4)return "nesting";
            *p+=7;ws_space(p);
            char num[67];size_t n=0;uint64_t count;
            while(**p&&**p!=' '&&**p!='{'){
                if(n==sizeof num-1)return "count";
                num[n++]=*(*p)++;
            }
            num[n]=0;const char *q=num;
            if(!tda_num(&q,&count)||*q||count<1||count>1024)return "count";
            ws_space(p);if(**p!='{')return "block";(*p)++;
            const char *start=*p,*after=NULL;
            for(uint64_t j=0;j<count;j++){
                if(!*fuel)return "step_limit";
                --*fuel;
                const char *body=start;
                const char *err=ws_block(&body,probe,r,depth+1,fuel);
                if(err)return err;
                if(*body!='}')return "block";
                after=body+1;
            }
            *p=after;ws_space(p);continue;
        }
        unsigned i;uint64_t v,*dest;char op;
        if(**p=='m'){
            if(!ws_slot(p,r,&i))return "bounds";
            dest=&r->mem[i];
        }else{
            if(!ws_reg(p,&i))return "register";
            dest=&r->r[i];
        }
        ws_space(p);op=**p;
        if(op=='=')(*p)++;
        else if(op&&strchr("+-*^&|/%",op)&&(*p)[1]=='=')*p+=2;
        else return "operator";
        if(!ws_value(p,r,probe,&v))return "value";
        ws_space(p);if(**p!=';')return "semicolon";(*p)++;
        switch(op){
            case '=':*dest=v;break;
            case '+':*dest+=v;break;
            case '-':*dest-=v;break;
            case '*':*dest*=v;break;
            case '^':*dest^=v;break;
            case '&':*dest&=v;break;
            case '|':*dest|=v;break;
            case '/':if(!v)return "division_zero";*dest/=v;break;
            case '%':if(!v)return "division_zero";*dest%=v;break;
        }
        r->steps++;ws_space(p);
    }
    return NULL;
}
static inline const char *ws_run(const char *p,WsProbe probe,WsResult *out){
    WsResult r={0};unsigned fuel=4096;
    if(strlen(p)>WS_SOURCE)return "length";
    const char *err=ws_block(&p,probe,&r,0,&fuel);
    if(err)return err;
    if(*p)return "block";
    if(!r.steps)return "empty";
    *out=r;return NULL;
}
#endif
