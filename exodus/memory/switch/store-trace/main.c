#include <stdio.h>
#include <switch.h>
int exodus_store_trace(FILE *out);
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
    puts("EXODUS integrated store trace (" LABEL ")\nY Run trace    + Exit");
    while(appletMainLoop()){
        padUpdate(&pad);
        u64 buttons=padGetButtonsDown(&pad);
        if(buttons&HidNpadButton_Plus)break;
        if(buttons&HidNpadButton_Y){
            consoleClear();puts("EXODUS store trace (" LABEL ")\nRunning...");
            consoleUpdate(NULL);
            FILE *out=fopen(PATH,"w");
            if(!out)puts("Cannot open CSV. No measurements taken.");
            else{
                int rc=exodus_store_trace(out);
                if(fclose(out)!=0)rc=7;
                if(rc)printf("FAILED (%d); discard CSV.\n",rc);
                else puts("Store trace passed; CSV saved.");
            }
            puts("Y Repeat    + Exit");
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
