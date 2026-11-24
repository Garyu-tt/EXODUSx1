#include "page_probe_stats.h"
#include "page_store.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#define SAMPLE_PATH "sdmc:/switch/exodus-page-probe/sample.bin"

static void make_demo_page(uint8_t *page, size_t index)
{
    memset(page, index == 2 ? 0x5a : 0, EXODUS_PAGE_BYTES);
    if (index == 1)
        page[0] = 1;
}

static void show_demo(void)
{
    static const char *const names[] = {"Zero page", "One live block", "Dense page"};
    uint8_t page[EXODUS_PAGE_BYTES], restored[EXODUS_PAGE_BYTES];
    uint8_t payload[2 * EXODUS_PAGE_BYTES];
    size_t ends[sizeof(names) / sizeof(names[0])];
    struct exodus_page_store store = {
        .payload = payload,
        .payload_capacity = sizeof(payload),
        .page_ends = ends,
        .page_capacity = sizeof(ends) / sizeof(ends[0])
    };
    puts("Generated 4096-byte pages");
    for (size_t i = 0; i < store.page_capacity; ++i) {
        make_demo_page(page, i);
        if (!exodus_page_store_append(&store, page)) {
            puts("Could not store generated pages");
            return;
        }
    }
    size_t start = 0;
    for (size_t i = 0; i < store.page_count; ++i) {
        make_demo_page(page, i);
        if (exodus_page_store_restore(&store, i, restored) && memcmp(page, restored, sizeof(page)) == 0)
            printf("%-16s %4zu payload bytes   byte check passed\n", names[i], ends[i] - start);
        else
            printf("%-16s round-trip verification FAILED\n", names[i]);
        start = ends[i];
    }
    printf("Packed group: %zu payload bytes + %zu index bytes\n",
           store.payload_used, store.page_count * sizeof(ends[0]));
}

static void show_measurement(const char *status, const struct exodus_probe_stats *stats)
{
    printf("%s\n\n", status);
    printf("Pages verified:       %" PRIu64 "\n", stats->pages);
    printf("Sparse pages:         %" PRIu64 "\n", stats->sparse_pages);
    printf("Input bytes counted:  %" PRIu64 "\n", stats->pages * EXODUS_PAGE_BYTES + stats->trailing_bytes);
    printf("Payload bytes:        %" PRIu64 "\n", stats->payload_bytes);
    printf("Length index bytes:   %" PRIu64 "\n", stats->pages * 2);
    printf("Trailing bytes (raw): %zu\n", stats->trailing_bytes);
}

int main(void)
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    struct exodus_probe_stats stats = {0};
    FILE *input = NULL;
    const char *status = NULL;
    bool redraw = true;

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 buttons = padGetButtonsDown(&pad);
        if (buttons & HidNpadButton_Plus)
            break;
        if (input && (buttons & HidNpadButton_B)) {
            fclose(input);
            input = NULL;
            status = "File scan cancelled. Partial totals below.";
            redraw = true;
        } else if (!input && (buttons & HidNpadButton_X)) {
            memset(&stats, 0, sizeof(stats));
            input = fopen(SAMPLE_PATH, "rb");
            status = input ? "Scanning file..." : "Could not open the sample file.";
            redraw = true;
        } else if (!input && (buttons & HidNpadButton_A)) {
            status = NULL;
            redraw = true;
        }

        if (input) {
            enum exodus_probe_result result = exodus_probe_batch(input, 128, &stats);
            if (result != EXODUS_PROBE_MORE) {
                if (result == EXODUS_PROBE_DONE)
                    status = "File scan complete. All full pages passed.";
                else if (result == EXODUS_PROBE_MISMATCH)
                    status = "Round-trip verification FAILED. Partial totals below.";
                else
                    status = "File read failed. Partial totals below.";
                if (fclose(input) != 0)
                    status = "File close failed. Results incomplete.";
                input = NULL;
            }
            redraw = true;
        }

        if (redraw) {
            consoleClear();
            puts("EXODUS sparse page probe\n");
            if (status)
                show_measurement(status, &stats);
            else
                show_demo();
            puts("\nOptional sample: " SAMPLE_PATH);
            puts(input ? "B  Cancel scan     +  Exit" : "A  Run demo     X  Scan sample     +  Exit");
            redraw = false;
        }
        consoleUpdate(NULL);
    }

    if (input)
        fclose(input);
    consoleExit(NULL);
    return EXIT_SUCCESS;
}
