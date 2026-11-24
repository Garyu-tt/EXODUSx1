/* One invocation per NRO launch: the donor shell owns global state. */
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int nxbsd_shell_main(int argc, char *argv[]);
void nxbsd_horizon_finish(int status) __attribute__((noreturn));
static int console_ready;
static void wait_for_exit(void)
{
    PadState pad;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while(appletMainLoop()) {
        padUpdate(&pad);
        if(padGetButtonsDown(&pad)&(HidNpadButton_Plus|HidNpadButton_B)) break;
        consoleUpdate(NULL);
    }
}
void nxbsd_horizon_finish(int status)
{
    if(console_ready) {
        fflush(stdout);
        fflush(stderr);
        printf("\nNX-BSD exit status: %d\nPress + or B to close.\n",status);
        consoleUpdate(NULL);
        wait_for_exit();
        consoleExit(NULL);
        console_ready=0;
    }
    exit(status);
}
int main(void)
{
    consoleInit(NULL);
    console_ready=1;
    puts("NX-BSD / FreeBSD sh - experimental one-shot console");
    puts("No POSIX process isolation, job control or signal delivery.");
    if(chdir("sdmc:/")) { perror("sdmc"); nxbsd_horizon_finish(1); }
    puts("Opening command keyboard. Commands may modify the SD card.");
    consoleUpdate(NULL);
    char line[1024]={0};
    SwkbdConfig cfg;
    Result rc=swkbdCreate(&cfg,0);
    if(R_FAILED(rc)) { puts("Keyboard unavailable."); nxbsd_horizon_finish(1); }
    swkbdConfigMakePresetDefault(&cfg);
    swkbdConfigSetHeaderText(&cfg,"FreeBSD sh command");
    swkbdConfigSetStringLenMax(&cfg,sizeof(line)-1);
    rc=swkbdShow(&cfg,line,sizeof(line));
    swkbdClose(&cfg);
    line[sizeof(line)-1]=0;
    if(R_FAILED(rc)||!line[0]) nxbsd_horizon_finish(0);
    /* No login or interactive profile loading, no automatic script execution. */
    unsetenv("ENV");
    char name[]="nxbsd-sh", option[]="-c";
    char *argv[]={name,option,line,NULL};
    nxbsd_horizon_finish(nxbsd_shell_main(3,argv));
}
