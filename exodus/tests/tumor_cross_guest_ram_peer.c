/* Host-side AArch64 reference peer; only disposable QEMU RAM file. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
int main(int argc,char **argv){
    if(argc!=4||sizeof(void*)!=8)return 1;
    if(strcmp(argv[1],"ram.bin"))return 2;
    long mib=strtol(argv[2],NULL,10);
    if(mib!=32&&mib!=64)return 3;
    off_t size=(off_t)mib*1024*1024;
    int fd=open(argv[1],O_RDWR);
    struct stat st;
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=size)return 4;
    uint32_t *p=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,size-4096);
    if(p==MAP_FAILED)return 5;
    close(fd);
    int result=0;
    if(!strcmp(argv[3],"init")){
        memset(p,0,4096);
        p[16]=0x12345678u;p[17]=~p[16];
        atomic_thread_fence(memory_order_release);
        p[0]=0x544d5231u;
        puts("EXODUS_ARM64_RAM_INIT");
    }else if(!strcmp(argv[3],"check")){
        if(p[48]!=1)result=6;
        atomic_thread_fence(memory_order_acquire);
        if(p[32]!=0x87654321u||p[33]!=~p[32])result=7;
        if(!result)puts("EXODUS_ARM64_RAM_PASS");
    }else result=8;
    if(munmap(p,4096))return 9;
    return result;
}
