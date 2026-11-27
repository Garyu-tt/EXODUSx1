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
typedef struct { uint64_t r[8]; unsigned steps; } WsResult;
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
static inline int ws_value(const char **p,const WsResult *r,WsProbe probe,uint64_t *v){
    ws_space(p);unsigned i;
    if(**p=='r'){if(!ws_reg(p,&i))return 0;*v=r->r[i];return 1;}
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
/* Grammar: rN = VALUE; rN += VALUE; rN *= VALUE; etc.
 * VALUE: literal, register, read-only probe(). All unsigned 64-bit.
 * A semicolon is mandatory. No loops, calls beyond probes, or native pointers. */
static inline const char *ws_run(const char *p,WsProbe probe,WsResult *out){
    WsResult r={0};
    if(strlen(p)>WS_SOURCE)return "length";
    ws_space(&p);
    while(*p){
        unsigned i;uint64_t v;char op;
        if(r.steps>=64)return "step_limit";
        if(!ws_reg(&p,&i))return "register";
        ws_space(&p);op=*p;
        if(op=='=')p++;
        else if(op&&strchr("+-*^&|",op)&&p[1]=='=')p+=2;
        else return "operator";
        if(!ws_value(&p,&r,probe,&v))return "value";
        ws_space(&p);if(*p!=';')return "semicolon";p++;
        switch(op){
            case '=':r.r[i]=v;break;
            case '+':r.r[i]+=v;break;
            case '-':r.r[i]-=v;break;
            case '*':r.r[i]*=v;break;
            case '^':r.r[i]^=v;break;
            case '&':r.r[i]&=v;break;
            case '|':r.r[i]|=v;break;
        }
        r.steps++;ws_space(&p);
    }
    if(!r.steps)return "empty";
    *out=r;return NULL;
}
#endif
