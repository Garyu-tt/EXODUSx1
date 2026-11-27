/* SPDX-License-Identifier: MIT
 * Ordinary Horizon application, no privileged register access. */
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static char logbuf[4096];
static size_t used;
static bool overflow;
static void logline(const char *fmt, ...) {
    if (overflow) return;
    va_list ap;
    va_start(ap, fmt);
    int n=vsnprintf(logbuf+used,sizeof logbuf-used,fmt,ap);
    va_end(ap);
    if(n<0||(size_t)n>=sizeof logbuf-used){overflow=true;return;}
    printf("%s",logbuf+used);used+=(size_t)n;
}
static void query(const char *name,u32 id) {
    u64 value=0;
    Result r=svcGetInfo(&value,id,CUR_PROCESS_HANDLE,0);
    logline("%s_rc=0x%08" PRIx32 "\n",name,(uint32_t)r);
    if(R_SUCCEEDED(r))logline("%s=%" PRIu64 "\n",name,(uint64_t)value);
}
typedef struct { u32 core, sum; } Work;
static Work results[4];
static void work(void *arg) {
    Work *w=arg;
    w->core=svcGetCurrentProcessorNumber();
    volatile u32 sum=0x12345678;
    for(unsigned i=0;i<10000;i++)sum=sum*1664525u+1013904223u;
    w->sum=sum;
}
static void core_test(int i) {
    static Thread threads[4];
    Work *w=&results[i];
    w->core=UINT32_MAX;w->sum=0;
    Result r=threadCreate(&threads[i],work,w,NULL,0x4000,0x2c,i);
    logline("core%d_create=0x%08" PRIx32 "\n",i,(uint32_t)r);
    if(R_FAILED(r))return;
    r=threadStart(&threads[i]);
    logline("core%d_start=0x%08" PRIx32 "\n",i,(uint32_t)r);
    if(R_SUCCEEDED(r)){
        r=threadWaitForExit(&threads[i]);
        logline("core%d_wait=0x%08" PRIx32 "\n",i,(uint32_t)r);
        if(R_FAILED(r))return;
        u32 expected=0x12345678;
        for(unsigned j=0;j<10000;j++)expected=expected*1664525u+1013904223u;
        logline("core%d_observed=%" PRIu32 "\ncore%d_compute=%s\n",i,(uint32_t)w->core,i,w->sum==expected?"pass":"fail");
    }
    threadClose(&threads[i]);
}
static void service_test(const char *label,const char *name) {
    Service s={0};
    Result r=smGetService(&s,name);
    logline("%s_open_rc=0x%08" PRIx32 "\n",label,(uint32_t)r);
    if(R_SUCCEEDED(r))serviceClose(&s);
}
int main(void) {
    consoleInit(NULL);
    logline("schema=1\nkind=hos_userland_inventory\n");
    u32 version=hosversionGet();
    logline("hos_version=%u.%u.%u\n",(unsigned)HOSVER_MAJOR(version),(unsigned)HOSVER_MINOR(version),(unsigned)HOSVER_MICRO(version));
    query("process_total_bytes",InfoType_TotalMemorySize);
    query("process_used_bytes",InfoType_UsedMemorySize);
    u64 mask=0;Result r=svcGetInfo(&mask,InfoType_CoreMask,CUR_PROCESS_HANDLE,0);
    logline("core_mask_rc=0x%08" PRIx32 "\n",(uint32_t)r);
    if(R_SUCCEEDED(r)){
        logline("core_mask=0x%" PRIx64 "\n",(uint64_t)mask);
        for(int i=0;i<4;i++)if(mask&(1ull<<i))core_test(i);
    }
    const size_t count=262144;
    volatile u32 *p=malloc(count*sizeof(u32));
    if(p){
        size_t errors=0;
        for(size_t i=0;i<count;i++)p[i]=(u32)i^0xa57a57a5u;
        for(size_t i=0;i<count;i++)if(p[i]!=((u32)i^0xa57a57a5u))errors++;
        for(size_t i=0;i<count;i++)p[i]=~((u32)i^0xa57a57a5u);
        for(size_t i=0;i<count;i++)if(p[i]!=~((u32)i^0xa57a57a5u))errors++;
        logline("own_ram_bytes=%zu\nown_ram_errors=%zu\n",count*sizeof(u32),errors);
        free((void*)p);
    }else logline("own_ram=allocation_failed\n");
    u64 before=armGetSystemTick();
    svcSleepThread(20000000);
    u64 after=armGetSystemTick();
    logline("user_timer_progress=%s\n",after>before?"pass":"fail");
    service_test("audren","audren:u");
    service_test("audout","audout:u");
    logline("adsp_execution=not_tested\na57_el2=not_tested\ndual_kernel=not_tested\ndeployment_authorized=0\n");
    if(!overflow){
        int fd=open("sdmc:/TUMORH01.TXT",O_WRONLY|O_CREAT|O_EXCL,0644);
        if(fd<0)printf("Report not saved (errno=%d)\n",errno);
        else{
            size_t off=0;
            while(off<used){ssize_t n=write(fd,logbuf+off,used-off);if(n<=0)break;off+=(size_t)n;}
            int cr=close(fd);
            printf("Report %s (%zu/%zu)\n",off==used&&cr==0?"saved":"incomplete",off,used);
        }
    }else printf("Report overflow; not saved\n");
    printf("PLUS: exit\n");
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad;padInitializeDefault(&pad);
    while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(NULL);}
    consoleExit(NULL);return 0;
}
