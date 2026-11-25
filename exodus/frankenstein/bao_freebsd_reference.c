/* Reference configuration for Bao v2.0.0, QEMU virt only.
 * API reference: Bao demos linux+freertos/configs/qemu-aarch64-virt.c.
 * No physical Tegra configuration or Nintendo binaries. */
#include <config.h>
VM_IMAGE(guest_a, XSTR(TUMOR_IMAGES/guest-A.bin))
VM_IMAGE(guest_b, XSTR(TUMOR_IMAGES/guest-B.bin))
#define BASE 0x60000000
#define SHARED 0xf0000000
#ifdef TUMOR_DUAL_FREEBSD
#define GUEST_MEM { .base = BASE, .size = 0x10000000 }
#else
#define GUEST_MEM { .base = BASE, .size = 0x4000000 }
#endif
#define GUEST_IPC { .base = SHARED, .size = 0x10000, .shmem_id = 0 }
#define GUEST_GIC { .gic = { .gicd_addr = 0x08000000, .gicr_addr = 0x080a0000 } }
struct config config = {
    CONFIG_HEADER
    .shmemlist_size = 1,
    .shmemlist = (struct shmem[]) { { .size = 0x10000 } },
    .vmlist_size = 2,
    .vmlist = (struct vm_config[]) {
        {
            .image = VM_IMAGE_BUILTIN(guest_a, BASE),
            .entry = BASE,
            .cpu_affinity = 0x7,
            .platform = {
                .cpu_num = 3,
                .region_num = 1,
#ifdef TUMOR_DUAL_FREEBSD
                .regions = (struct vm_mem_region[]) { GUEST_MEM },
                .dev_num = 1,
                .devs = (struct vm_dev_region[]) {
                    { .interrupt_num = 1, .interrupts = (irqid_t[]) {27} },
                },
#else
                .regions = (struct vm_mem_region[]) { GUEST_MEM },
#endif
                .ipc_num = 1,
                .ipcs = (struct ipc[]) { GUEST_IPC },
                .arch = GUEST_GIC,
            },
        },
        {
            .image = VM_IMAGE_BUILTIN(guest_b, BASE),
            .entry = BASE,
            .cpu_affinity = 0x8,
            .platform = {
                .cpu_num = 1,
                .region_num = 1,
                .regions = (struct vm_mem_region[]) { { .base = BASE, .size = 0x20000000 } },
                .ipc_num = 1,
                .ipcs = (struct ipc[]) { GUEST_IPC },
                .dev_num = 2,
                .devs = (struct vm_dev_region[]) {
                    { .pa = 0x09000000, .va = 0x09000000, .size = 0x10000,
                      .interrupt_num = 1, .interrupts = (irqid_t[]) {33} },
                    { .interrupt_num = 1, .interrupts = (irqid_t[]) {27} },
                },
                .arch = GUEST_GIC,
            },
        },
    },
};
