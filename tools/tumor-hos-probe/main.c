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
#include <malloc.h>

static char logbuf[8192];
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

/* R02: finite concurrent workload; only application-owned memory. */
#include <stdatomic.h>
#include <string.h>
#define ROUNDS 2000000u
typedef struct {
    u32 requested, observed, sum;
    u64 begin, end;
    u32 peer_value;
    bool peer_ok;
    _Atomic u32 published;
} Parallel;
static Parallel jobs[4];
static Thread pool[4];
static _Atomic unsigned gate;
static unsigned active[4], nactive;

static void parallel_worker(void *arg) {
    Parallel *j=arg;
    /* Bounded start wait, including partial thread-start failure. */
    u64 deadline=armGetSystemTick()+armNsToTicks(3000000000ull);
    unsigned state;
    while(!(state=atomic_load_explicit(&gate,memory_order_acquire))){
        if(armGetSystemTick()>=deadline)return;
        svcSleepThread(100000);
    }
    if(state!=1)return;
    j->observed=svcGetCurrentProcessorNumber();
    j->begin=armGetSystemTick();
    volatile u32 x=0x12345678u;
    for(unsigned k=0;k<ROUNDS;k++)x=x*1664525u+1013904223u;
    j->sum=x;
    j->end=armGetSystemTick();
    j->peer_value=0xA5700000u+j->requested;
    atomic_store_explicit(&j->published,1,memory_order_release);
    /* Read every active peer's ordinary data after acquiring its flag. */
    deadline=armGetSystemTick()+armNsToTicks(3000000000ull);
    j->peer_ok=true;
    for(unsigned k=0;k<nactive;k++){
        Parallel *p=&jobs[active[k]];
        while(!atomic_load_explicit(&p->published,memory_order_acquire)){
            if(armGetSystemTick()>=deadline){j->peer_ok=false;return;}
            svcSleepThread(100000);
        }
        if(p->peer_value!=0xA5700000u+p->requested)j->peer_ok=false;
    }
}
static void parallel_test(u64 mask) {
    unsigned created=0,started=0;
    nactive=0;
    for(unsigned i=0;i<4;i++)if(mask&(1ull<<i))active[nactive++]=i;
    if(!nactive){logline("parallel=not_tested\n");return;}
    atomic_store(&gate,0);
    for(unsigned k=0;k<nactive;k++){
        unsigned i=active[k];Parallel *j=&jobs[i];
        j->requested=i;j->observed=UINT32_MAX;
        atomic_store(&j->published,0);
        Result r=threadCreate(&pool[i],parallel_worker,j,NULL,0x4000,0x2c,i);
        logline("p%u_create=0x%08" PRIx32 "\n",i,(uint32_t)r);
        if(R_FAILED(r))break;
        created++;
        r=threadStart(&pool[i]);
        logline("p%u_start=0x%08" PRIx32 "\n",i,(uint32_t)r);
        if(R_FAILED(r))break;
        started++;
    }
    atomic_store_explicit(&gate,started==nactive?1:2,memory_order_release);
    bool ok=started==nactive, joined=true;
    for(unsigned k=0;k<started;k++){
        unsigned i=active[k];Result r=threadWaitForExit(&pool[i]);
        logline("p%u_wait=0x%08" PRIx32 "\n",i,(uint32_t)r);
        if(R_FAILED(r)){joined=false;ok=false;}
        else threadClose(&pool[i]);
    }
    if(created>started)threadClose(&pool[active[started]]);
    if(!joined){logline("parallel=wait_failed\n");return;}
    if(started!=nactive){logline("parallel=incomplete\n");return;}
    u32 expected=0x12345678u;
    for(unsigned k=0;k<ROUNDS;k++)expected=expected*1664525u+1013904223u;
    u64 latest=0,earliest=UINT64_MAX;
    for(unsigned k=0;k<nactive;k++){
        unsigned i=active[k];Parallel *j=&jobs[i];
        bool pass=j->observed==i&&j->sum==expected&&j->end>j->begin&&j->peer_ok;
        ok=ok&&pass;
        if(j->begin>latest)latest=j->begin;
        if(j->end<earliest)earliest=j->end;
        logline("p%u_core=%" PRIu32 "\np%u_compute=%s\np%u_peer=%s\np%u_begin=%" PRIu64 "\np%u_end=%" PRIu64 "\n",i,(uint32_t)j->observed,i,j->sum==expected?"pass":"fail",i,j->peer_ok?"pass":"fail",i,(uint64_t)j->begin,i,(uint64_t)j->end);
    }
    logline("parallel_workers=%u\nparallel=%s\nall_worker_intervals_overlap=%s\n",nactive,ok?"pass":"fail",nactive>1&&earliest>latest?"yes":"no");
}
static void bandwidth_test(void) {
    const size_t bytes=4*1024*1024;
    unsigned char *a=malloc(bytes),*b=malloc(bytes);
    if(!a||!b){free(a);free(b);logline("copy=allocation_failed\n");return;}
    for(size_t i=0;i<bytes;i++)a[i]=(unsigned char)(i^(i>>8));
    /* Four independent full-buffer copies, using volatile function pointer
       to prevent optimizer folding the repeated calls. */
    void *(*volatile copy_fn)(void *,const void *,size_t)=memcpy;
    u64 start=armGetSystemTick();
    for(unsigned k=0;k<4;k++)copy_fn(b,a,bytes);
    u64 elapsed=armGetSystemTick()-start;
    bool ok=memcmp(a,b,bytes)==0;
    logline("copy_bytes=%zu\ncopy_ticks=%" PRIu64 "\ncopy_ns=%" PRIu64 "\ncopy_integrity=%s\n",bytes*4,(uint64_t)elapsed,(uint64_t)armTicksToNs(elapsed),ok?"pass":"fail");
    free(a);free(b);
}


/* R03: ordinary audio renderer session, silent PCM, bounded frame waits. */
static void audio_trial(unsigned voices) {
    AudioRendererConfig cfg={
        .output_rate=AudioRendererOutputRate_48kHz,
        .num_voices=voices,.num_effects=0,.num_sinks=1,
        .num_mix_objs=1,.num_mix_buffers=2
    };
    enum { SAMPLES=24000, POOL_SIZE=49152 };
    AudioDriver drv={0};
    AudioDriverWaveBuf waves[8]={0};
    void *pcm=memalign(0x1000,POOL_SIZE);
    if(!pcm){logline("audio%u=allocation_failed\n",voices);return;}
    memset(pcm,0,POOL_SIZE);
    armDCacheFlush(pcm,POOL_SIZE);
    bool driver=false,started=false,ok=false;
    unsigned frames=0,completed=0,drops=0;
    u64 begin=0,elapsed=0;
    Result rc=audrenInitialize(&cfg);
    logline("audio%u_init_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);
    if(R_FAILED(rc)){free(pcm);return;}
    rc=audrvCreate(&drv,&cfg,2);
    logline("audio%u_driver_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);
    if(R_FAILED(rc))goto cleanup;
    driver=true;
    int pool=audrvMemPoolAdd(&drv,pcm,POOL_SIZE);
    if(pool<0||!audrvMemPoolAttach(&drv,pool))goto cleanup;
    static const u8 channels[]={0,1};
    if(audrvDeviceSinkAdd(&drv,AUDREN_DEFAULT_DEVICE_NAME,2,channels)<0)goto cleanup;
    for(unsigned i=0;i<voices;i++){
        if(!audrvVoiceInit(&drv,i,1,PcmFormat_Int16,48000))goto cleanup;
        audrvVoiceSetDestinationMix(&drv,i,AUDREN_FINAL_MIX_ID);
        audrvVoiceSetMixFactor(&drv,i,1.0f,0,0);
        audrvVoiceSetMixFactor(&drv,i,1.0f,0,1);
        waves[i].data_raw=pcm;
        waves[i].size=SAMPLES*sizeof(int16_t);
        waves[i].end_sample_offset=SAMPLES;
        if(!audrvVoiceAddWaveBuf(&drv,i,&waves[i]))goto cleanup;
        audrvVoiceStart(&drv,i);
    }
    rc=audrvUpdate(&drv);
    logline("audio%u_submit_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);
    if(R_FAILED(rc))goto cleanup;
    rc=audrenStartAudioRenderer();
    logline("audio%u_start_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);
    if(R_FAILED(rc))goto cleanup;
    started=true;
    begin=armGetSystemTick();
    Event *event=audrenGetFrameEvent();
    if(!event)goto cleanup;
    for(unsigned n=0;n<300;n++){
        if(armGetSystemTick()-begin>armNsToTicks(2500000000ull))break;
        rc=eventWait(event,100000000ull);
        if(R_FAILED(rc)){logline("audio%u_event_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);break;}
        frames++;
        rc=audrvUpdate(&drv);
        if(R_FAILED(rc)){logline("audio%u_update_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);break;}
        completed=0;
        for(unsigned i=0;i<voices;i++)if(waves[i].state==AudioDriverWaveBufState_Done)completed++;
        if(completed==voices)break;
    }
    elapsed=armGetSystemTick()-begin;
    for(unsigned i=0;i<voices;i++){
        u32 samples=audrvVoiceGetPlayedSampleCount(&drv,i);
        u32 dropped=audrvVoiceGetVoiceDropsCount(&drv,i);
        drops+=dropped;
        logline("audio%u_voice%u_samples=%" PRIu32 "\n",voices,i,(uint32_t)samples);
    }
    ok=completed==voices&&drops==0&&frames>0;
cleanup:
    if(started){
        rc=audrenStopAudioRenderer();
        logline("audio%u_stop_rc=0x%08" PRIx32 "\n",voices,(uint32_t)rc);
        if(R_FAILED(rc))ok=false;
    }
    if(driver)audrvClose(&drv);
    audrenExit();
    free(pcm);
    logline("audio%u_frames=%u\naudio%u_completed=%u\naudio%u_drops=%u\naudio%u_elapsed_ns=%" PRIu64 "\naudio%u=%s\n",voices,frames,voices,completed,voices,drops,voices,(uint64_t)armTicksToNs(elapsed),voices,ok?"pass":"incomplete");
}

int main(int argc,char **argv) {
    consoleInit(NULL);
    bool remote=false;
    for(int i=1;i<argc;i++)if(strcmp(argv[i],"--remote")==0)remote=true;
    bool sockets=false;
    int remote_fd=-1;
    if(remote){
        sockets=R_SUCCEEDED(socketInitializeDefault());
        if(sockets)remote_fd=nxlinkStdioForDebug();
        printf("NXLink report transport: %s\n",remote_fd>=0?"connected":"local fallback");
        consoleUpdate(NULL);
    }
    logline("schema=1\nkind=hos_userland_inventory\nprobe_revision=4\n");
    u32 version=hosversionGet();
    logline("hos_version=%u.%u.%u\n",(unsigned)HOSVER_MAJOR(version),(unsigned)HOSVER_MINOR(version),(unsigned)HOSVER_MICRO(version));
    query("process_total_bytes",InfoType_TotalMemorySize);
    query("process_used_bytes",InfoType_UsedMemorySize);
    u64 mask=0;Result r=svcGetInfo(&mask,InfoType_CoreMask,CUR_PROCESS_HANDLE,0);
    logline("core_mask_rc=0x%08" PRIx32 "\n",(uint32_t)r);
    if(R_SUCCEEDED(r)){
        logline("core_mask=0x%" PRIx64 "\n",(uint64_t)mask);
        for(int i=0;i<4;i++)if(mask&(1ull<<i))core_test(i);
        parallel_test(mask);
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
    bandwidth_test();
    service_test("audren","audren:u");
    service_test("audout","audout:u");
    audio_trial(1);
    audio_trial(4);
    audio_trial(8);
    logline("adsp_execution=not_tested\na57_el2=not_tested\ndual_kernel=not_tested\ndeployment_authorized=0\n");
    if(!overflow){
        int fd=open("sdmc:/TUMORH04.TXT",O_WRONLY|O_CREAT|O_EXCL,0644);
        if(fd<0)printf("Report not saved (errno=%d)\n",errno);
        else{
            size_t off=0;
            while(off<used){ssize_t n=write(fd,logbuf+off,used-off);if(n<=0)break;off+=(size_t)n;}
            int cr=close(fd);
            printf("Report %s (%zu/%zu)\n",off==used&&cr==0?"saved":"incomplete",off,used);
        }
    }else printf("Report overflow; not saved\n");
    bool sent=false;
    if(remote_fd>=0&&!overflow){
        int b=fprintf(stderr,"TDA_REPORT_BEGIN %zu\n",used);
        size_t written=fwrite(logbuf,1,used,stderr);
        int e=fprintf(stderr,"TDA_REPORT_END\n");
        sent=b>0&&written==used&&e>0&&fflush(stderr)==0;
    }
    if(remote_fd>=0)close(remote_fd);
    if(sockets)socketExit();
    if(sent){
        consoleExit(NULL);
        return 0;
    }
    printf("PLUS: exit\n");
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad;padInitializeDefault(&pad);
    while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(NULL);}
    consoleExit(NULL);return 0;
}
