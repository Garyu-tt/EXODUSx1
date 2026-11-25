#include <switch.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <switch/kernel/jit.h>

/* AArch64: mov w0, #42; ret. No system calls or external memory access. */
static const uint32_t return_42[] = {0x52800540u, 0xD65F03C0u};
typedef int (*jit_entry_t)(void);

static int finish(int status)
{
    puts("Press PLUS to exit.");
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return status;
}

int main(void)
{
    Jit jit = {0};
    Result rc;
    int value;
    bool hints_code_memory = envIsSyscallHinted(0x4B) && envIsSyscallHinted(0x4C);
    bool hints_process_memory = envIsSyscallHinted(0x73) &&
                               envIsSyscallHinted(0x77) &&
                               envIsSyscallHinted(0x78);

    consoleInit(NULL);
    printf("EXODUS NX JIT probe\n");
    printf("HOS version: 0x%08X\n", hosversionGet());
    printf("Own process handle: 0x%08X\n", envGetOwnProcessHandle());
    printf("CodeMemory hints (4B/4C): %s\n", hints_code_memory ? "yes" : "no");
    printf("Process-code hints (73/77/78): %s\n",
           hints_process_memory ? "yes" : "no");

    rc = jitCreate(&jit, 0x1000);
    if (R_FAILED(rc)) {
        printf("jitCreate unavailable/failed: 0x%08X\n", (unsigned)rc);
        return finish(77);
    }

    printf("JIT type: %s\n", jit.type == JitType_CodeMemory ?
           "CodeMemory dual alias" : "SetProcessMemoryPermission");
    printf("RW alias: %p\n", jitGetRwAddr(&jit));
    printf("RX alias: %p\n", jitGetRxAddr(&jit));

    memcpy(jitGetRwAddr(&jit), return_42, sizeof(return_42));
    rc = jitTransitionToExecutable(&jit);
    if (R_FAILED(rc)) {
        printf("jitTransitionToExecutable failed: 0x%08X\n", (unsigned)rc);
        Result close_rc = jitClose(&jit);
        if (R_FAILED(close_rc))
            printf("jitClose after transition failure: 0x%08X\n", (unsigned)close_rc);
        return finish(1);
    }

    value = ((jit_entry_t)jitGetRxAddr(&jit))();
    printf("Generated function returned: %d\n", value);

    rc = jitClose(&jit);
    if (R_FAILED(rc)) {
        printf("jitClose failed: 0x%08X\n", (unsigned)rc);
        return finish(1);
    }

    printf("JIT buffer closed cleanly\n");
    return finish(value == 42 ? 0 : 1);
}
