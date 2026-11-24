#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <switch/kernel/virtmem.h>
#include <switch/kernel/svc.h>

static void report_region(const char *label, const void *address)
{
    MemoryInfo info = {0};
    u32 page_info = 0;
    Result rc = svcQueryMemory(&info, &page_info, (u64)address);
    if (R_FAILED(rc)) {
        printf("%s query failed: 0x%08X\n", label, (unsigned)rc);
        return;
    }
    printf("%s @ %p\n", label, address);
    printf("  region: %p + 0x%llX\n", (void *)(uintptr_t)info.addr,
           (unsigned long long)info.size);
    printf("  type=0x%08X attr=0x%08X perm=0x%08X page=0x%08X\n",
           info.type, info.attr, info.perm, page_info);
}

int main(void)
{
    void *page;
    void *candidate;
    VirtmemReservation *reservation = NULL;

    consoleInit(NULL);
    printf("EXODUS NX memory probe (read-only)\n");
    printf("HOS version: 0x%08X\n", hosversionGet());
    printf("Own process handle: 0x%08X\n", envGetOwnProcessHandle());
    printf("JIT hints: CodeMemory=%s ProcessCode=%s\n",
           envIsSyscallHinted(0x4B) && envIsSyscallHinted(0x4C) ? "yes" : "no",
           envIsSyscallHinted(0x73) && envIsSyscallHinted(0x77) &&
           envIsSyscallHinted(0x78) ? "yes" : "no");

    page = aligned_alloc(0x1000, 0x1000);
    if (page) {
        report_region("aligned heap page", page);
        free(page);
    } else {
        puts("aligned_alloc failed");
    }

    virtmemLock();
    candidate = virtmemFindAslr(0x1000, 0x1000);
    if (candidate)
        reservation = virtmemAddReservation(candidate, 0x1000);
    virtmemUnlock();

    if (candidate && reservation) {
        report_region("free ASLR candidate (bookkeeping-reserved, not mapped)", candidate);
        virtmemLock();
        virtmemRemoveReservation(reservation);
        virtmemUnlock();
    } else {
        puts("No free ASLR candidate/reservation returned");
    }

    puts("No kernel mappings or permissions were changed.");
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
    return 0;
}
