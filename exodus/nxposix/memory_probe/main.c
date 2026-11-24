#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <switch/kernel/virtmem.h>
#include <switch/kernel/svc.h>

static void probe_heap_readonly(void *page)
{
    volatile uint32_t *word = (volatile uint32_t *)page;
    Result rc;
    *word = 0x584F4458u; /* "XODX" sentinel while the page is RW. */
    rc = svcSetMemoryPermission(page, 0x1000, Perm_R);
    if (R_FAILED(rc)) {
        printf("heap RW->R protection unsupported: 0x%08X\n", (unsigned)rc);
        return;
    }

    printf("heap page became read-only; sentinel=0x%08X\n", *word);
    rc = svcSetMemoryPermission(page, 0x1000, Perm_Rw);
    if (R_FAILED(rc)) {
        printf("FATAL: failed to restore heap page RW: 0x%08X\n", (unsigned)rc);
        svcExitProcess();
    }
    *word = 0x52575257u;
    puts("heap page RW restore and write passed");
}

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
        MemoryInfo info = {0};
        u32 page_info = 0;
        report_region("aligned heap page", page);
        if (R_SUCCEEDED(svcQueryMemory(&info, &page_info, (u64)page)) &&
            info.type == MemType_Heap && info.perm == Perm_Rw &&
            !(info.attr & (MemAttr_IsBorrowed | MemAttr_IsUncached))) {
            probe_heap_readonly(page);
        } else {
            puts("heap page does not meet safe RW heap probe preconditions");
        }
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
