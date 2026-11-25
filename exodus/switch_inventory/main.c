#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <switch.h>

#define INVENTORY_DIR "sdmc:/switch/exodus-switch-inventory"
#define INVENTORY_PATH_SIZE sizeof(INVENTORY_DIR "/switch-titles-9999.json")
#define RECORD_BATCH 32
#define MAX_RECORDS 4096

static u64 title_ids[MAX_RECORDS];
static NsApplicationControlData control_data;

static int compare_title_ids(const void *first, const void *second)
{
    const u64 left = *(const u64 *)first;
    const u64 right = *(const u64 *)second;
    return (left > right) - (left < right);
}

static bool collect_title_ids(size_t *total)
{
    NsApplicationRecord records[RECORD_BATCH];
    s32 offset = 0;
    for (;;) {
        s32 count = 0;
        Result result = nsListApplicationRecord(records, RECORD_BATCH, offset, &count);
        if (R_FAILED(result)) {
            printf("Could not list Switch applications (0x%08" PRIx32 ").\n", (uint32_t)result);
            return false;
        }
        if (count < 0 || count > RECORD_BATCH || count > MAX_RECORDS - offset) {
            puts("Switch application count is outside the export limit.");
            return false;
        }
        if (count == 0)
            break;
        for (s32 i = 0; i < count; ++i) {
            if (records[i].application_id == 0) {
                puts("Received an invalid Switch title ID.");
                return false;
            }
            title_ids[offset + i] = records[i].application_id;
        }
        offset += count;
    }

    *total = (size_t)offset;
    qsort(title_ids, *total, sizeof(title_ids[0]), compare_title_ids);
    for (size_t i = 1; i < *total; ++i) {
        if (title_ids[i - 1] == title_ids[i]) {
            puts("Received a duplicate Switch title ID. Try the export again.");
            return false;
        }
    }
    return true;
}

static const char *read_title_name(u64 title_id)
{
    u64 size = 0;
    Result result = nsGetApplicationControlData(
        NsApplicationControlSource_Storage, title_id, &control_data, sizeof(control_data), &size);
    if (R_FAILED(result) || size < sizeof(control_data.nacp))
        return NULL;

    NacpLanguageEntry *language = NULL;
    result = nsGetApplicationDesiredLanguage(&control_data.nacp, &language);
    if (R_FAILED(result) || language == NULL || language->name[0] == '\0' ||
        memchr(language->name, '\0', sizeof(language->name)) == NULL ||
        utf8_to_utf32(NULL, (const uint8_t *)language->name, 0) < 0)
        return NULL;
    return language->name;
}

static void write_json_string(FILE *output, const char *value)
{
    fputc('"', output);
    for (const unsigned char *ch = (const unsigned char *)value; *ch; ++ch) {
        if (*ch == '"' || *ch == '\\') {
            fputc('\\', output);
            fputc(*ch, output);
        } else if (*ch < 0x20) {
            fprintf(output, "\\u%04x", (unsigned int)*ch);
        } else {
            fputc(*ch, output);
        }
    }
    fputc('"', output);
}

static bool write_inventory(FILE *output, size_t total, size_t *unnamed)
{
    if (fputs("{\"schema_version\":1,\"entries\":[\n", output) == EOF)
        return false;
    for (size_t i = 0; i < total; ++i) {
        char title_id[17];
        snprintf(title_id, sizeof(title_id), "%016" PRIx64, (uint64_t)title_ids[i]);
        const char *name = read_title_name(title_ids[i]);
        if (name == NULL) {
            ++*unnamed;
            name = title_id;
        }
        fprintf(output, "%s{\"provider\":\"switch\",\"provider_id\":\"%s\",\"name\":",
                i == 0 ? "" : ",\n", title_id);
        write_json_string(output, name);
        if (fputs(",\"artwork\":null,\"installation_state\":\"unknown\","
                  "\"last_played\":null,\"compatibility_state\":\"unknown\",\"launch_mode\":null}",
                  output) == EOF || ferror(output))
            return false;
    }
    return fputs("\n]}\n", output) != EOF;
}

static bool choose_inventory_path(char path[INVENTORY_PATH_SIZE])
{
    for (unsigned int number = 1; number <= 9999; ++number) {
        snprintf(path, INVENTORY_PATH_SIZE, INVENTORY_DIR "/switch-titles-%04u.json", number);
        struct stat existing;
        if (stat(path, &existing) == 0)
            continue;
        if (errno == ENOENT)
            return true;
        printf("Could not check the export path (%s).\n", strerror(errno));
        return false;
    }
    puts("Move earlier exports out of the inventory directory before exporting again.");
    return false;
}

static void export_inventory(void)
{
    char output_path[INVENTORY_PATH_SIZE];
    if (!choose_inventory_path(output_path))
        return;

    char pending_path[] = INVENTORY_DIR "/.switch-titles-XXXXXX";
    int descriptor = mkstemp(pending_path);
    if (descriptor < 0) {
        printf("Could not create a temporary export on the SD card (%s).\n", strerror(errno));
        return;
    }
    FILE *output = fdopen(descriptor, "wb");
    if (output == NULL) {
        close(descriptor);
        remove(pending_path);
        puts("Could not open the temporary export.");
        return;
    }

    size_t total = 0;
    size_t unnamed = 0;
    bool complete = collect_title_ids(&total) && write_inventory(output, total, &unnamed) &&
                    fflush(output) == 0 && fsync(descriptor) == 0;
    if (fclose(output) != 0)
        complete = false;
    // Horizon's SD rename refuses an existing destination.
    if (!complete || rename(pending_path, output_path) != 0) {
        remove(pending_path);
        puts("The new export could not be completed.");
        return;
    }
    printf("Exported %zu Switch application records.\n", total);
    if (unnamed != 0)
        printf("Used title IDs for %zu records with unavailable names.\n", unnamed);
    puts(output_path);
}

int main(void)
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    puts("EXODUS Switch Inventory\n\nA  Export    +  Exit");

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 buttons = padGetButtonsDown(&pad);
        if (buttons & HidNpadButton_Plus)
            break;
        if (buttons & HidNpadButton_A) {
            consoleClear();
            puts("Reading Switch application records...\n");
            consoleUpdate(NULL);
            Result result = nsInitialize();
            if (R_SUCCEEDED(result)) {
                export_inventory();
                nsExit();
            } else {
                printf("Switch application service unavailable (0x%08" PRIx32 ").\n", (uint32_t)result);
            }
            puts("\nA  Export    +  Exit");
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return EXIT_SUCCESS;
}
