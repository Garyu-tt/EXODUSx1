#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
int exodus_bench_store_trace(FILE *out);
#ifdef EXODUS_BENCH_BASELINE
#define LABEL "Baseline"
#define PATH "sdmc:/switch/exodus-page-probe/ex005-store-baseline.csv"
#else
#define LABEL "Candidate"
#define PATH "sdmc:/switch/exodus-page-probe/ex005-store-candidate.csv"
#endif
int main(void){
    consoleInit(NULL);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    PadState pad;padInitializeDefault(&pad);
    puts("EXODUS integrated store trace (" LABEL ")\nY Run  + Exit\nCSV: " PATH);
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 buttons=padGetButtonsDown(&pad);
        if(buttons&HidNpadButton_Plus)break;
        if(buttons&HidNpadButton_Y){
            consoleClear();puts("EXODUS integrated store trace (" LABEL ")\nRunning...");
            consoleUpdate(NULL);
            FILE *f=fopen(PATH,"w");
            if(!f)puts("Cannot open CSV");
            else{
                int rc=exodus_bench_store_trace(f);
                if(fclose(f)!=0)rc=7;
                if(rc){remove(PATH);printf("FAILED (%d); CSV invalid\n",rc);}
                else puts("Benchmark passed; CSV saved.");
            }
            puts("Y Run again  + Exit");
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return EXIT_SUCCESS;
}
