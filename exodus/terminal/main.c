/* EXODUS native terminal R03: local console and explicit NRO handoff. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

#define LINE_CAP 512
#define READ_CAP 4096
#define LIST_CAP 128

/* Render ASCII safely; never interpret escape sequences from files or names. */
static void safe_bytes(const unsigned char *p, size_t n, int multiline) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = p[i];
        if ((c >= 32 && c <= 126) || (multiline && (c == '\n' || c == '\t')))
            putchar(c);
        else printf("\\x%02x", (unsigned)c);
    }
}
static void safe_text(const char *s) {
    safe_bytes((const unsigned char *)s, strlen(s), 0);
}
static void report_error(const char *op) {
    printf("%s: error %d\n", op, errno);
}
static int valid_path(const char *p) {
    if (!*p) return 0;
#ifdef __SWITCH__
    /* Avoid selecting unrelated devoptab devices. */
    if (!strncmp(p, "sdmc:/", 6)) p += 6;
    if (strchr(p, ':')) return 0;
#endif
    return 1;
}
static void pwd(void) {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) report_error("pwd");
    else { safe_text(cwd); putchar('\n'); }
}
static void list(const char *path) {
    DIR *d = opendir(path);
    if (!d) { report_error("ls"); return; }
    unsigned count = 0;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(d);
        if (!e) { if (errno) report_error("ls"); break; }
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (count == LIST_CAP) { puts("[listing limited]"); break; }
        safe_text(e->d_name);
        putchar('\n');
        ++count;
    }
    closedir(d);
}
static void cat(const char *path) {
    struct stat st;
    if (stat(path, &st)) { report_error("cat"); return; }
    if (!S_ISREG(st.st_mode)) { puts("cat: regular files only"); return; }
    FILE *f = fopen(path, "rb");
    if (!f) { report_error("cat"); return; }
    unsigned char buf[512];
    size_t total = 0;
    while (total < READ_CAP) {
        size_t want = READ_CAP - total;
        if (want > sizeof(buf)) want = sizeof(buf);
        size_t n = fread(buf, 1, want, f);
        safe_bytes(buf, n, 1);
        total += n;
        if (n < want) break;
    }
    if (ferror(f)) report_error("cat");
    else if (total == READ_CAP && fgetc(f) != EOF) puts("\n[output limited]");
    putchar('\n');
    fclose(f);
}
static void file_info(const char *path) {
    struct stat st;
    if (stat(path, &st)) { report_error("stat"); return; }
    const char *type = S_ISREG(st.st_mode) ? "file" :
                       S_ISDIR(st.st_mode) ? "directory" : "other";
    printf("type=%s size_bytes=%" PRIdMAX "\n", type, (intmax_t)st.st_size);
}
#ifdef __SWITCH__
/* Same read-only SVC queries as Kernel Bridge admission R02 (#31).
   Adapter remains temporary until shared provider #32 is merged. */
static void observed(const char *name, Result rc, uint64_t value) {
    if (R_SUCCEEDED(rc)) printf("%s=%" PRIu64 "\n", name, value);
    else printf("%s=unknown result=0x%08" PRIx32 "\n", name, (uint32_t)rc);
}
#endif
static void identity(void) {
#ifdef __SWITCH__
    uint64_t value = 0;
    Result rc = svcGetProcessId(&value, CUR_PROCESS_HANDLE);
    observed("process_id", rc, value);
    value = 0;
    rc = svcGetThreadId(&value, CUR_THREAD_HANDLE);
    observed("thread_id", rc, value);
#else
    printf("process_id=%" PRIdMAX "\n", (intmax_t)getpid());
    puts("thread_id=unknown (host reference)");
#endif
}
static void memory_info(void) {
#ifdef __SWITCH__
    uint64_t value = 0;
    Result rc = svcGetInfo(&value, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    observed("process_memory_limit_bytes", rc, value);
    value = 0;
    rc = svcGetInfo(&value, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    observed("process_memory_used_bytes", rc, value);
#else
    puts("process_memory_limit_bytes=unknown (host reference)");
    puts("process_memory_used_bytes=unknown (host reference)");
#endif
    puts("Scope: current process only; not system RAM or GPU VRAM.");
}
/* Temporary preflight while the shared NRO admission code in #11 is pending.
   Structural checks only, not trust, signatures or loader compatibility. */
static uint32_t nro_u32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int nro_valid(const char *path) {
    struct stat st;
    unsigned char h[128];
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size < 128) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(h, 1, sizeof(h), f);
    int failed = ferror(f);
    if (fclose(f) || failed || n != sizeof(h) || memcmp(h+16, "NRO0", 4)) return 0;
    uint32_t size = nro_u32(h+24), end = 0;
    if (size < 128 || size % 4096 || size > (uintmax_t)st.st_size) return 0;
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t off = nro_u32(h+32+i*8), len = nro_u32(h+36+i*8);
        if ((i == 0 && (off || len < 128)) || off < end || off > size ||
            len > size-off || off % 4096 || len % 4096) return 0;
        end = off+len;
    }
    return 1;
}
static int run_app(const char *name) {
    /* Explicit known aliases avoid loader argv quoting ambiguity and implicit
       path search. User chooses locally installed code; no download or install. */
    const char *path = NULL;
    if (!strcmp(name, "ssh")) path = "sdmc:/switch/exodus-ssh.nro";
    else if (!strcmp(name, "autorun")) path = "sdmc:/switch/wine/wine-nx-runtime.nro";
    else { puts("unknown app; use apps"); return 0; }
#ifdef __SWITCH__
    if (appletGetAppletType() != AppletType_Application || !envHasNextLoad()) {
        puts("run: application mode and compatible loader required"); return 0;
    }
    if (!nro_valid(path)) { puts("run: missing or invalid NRO"); return 0; }
    Result rc = envSetNextLoad(path, path);
    if (R_FAILED(rc)) { printf("handoff failed: 0x%08" PRIx32 "\n", (uint32_t)rc); return 0; }
    puts("Handoff scheduled; exiting terminal.");
    return 1;
#else
    printf("run: native loader unavailable (host reference); target=%s\n", path);
    return 0;
#endif
}
static int dispatch(char *line) {
    size_t n = strlen(line);
    while (n && (line[n-1] == '\r' || line[n-1] == '\n')) line[--n] = 0;
    char *cmd = line;
    while (*cmd == ' ' || *cmd == '\t') ++cmd;
    char *arg = cmd + strcspn(cmd, " \t");
    if (*arg) { *arg++ = 0; while (*arg == ' ' || *arg == '\t') ++arg; }
    if (!*cmd) return 0;
    if (!strcmp(cmd, "exit") && !*arg) return 1;
    if (!strcmp(cmd, "help") && !*arg)
        puts("help | pwd | ls [path] | cd path | cat path | stat path | echo text | id | mem | ps | svc | apps | nro path | run NAME | status | clear | exit");
    else if (!strcmp(cmd, "pwd") && !*arg) pwd();
    else if (!strcmp(cmd, "ls")) {
        if (!*arg || valid_path(arg)) list(*arg ? arg : ".");
        else puts("invalid path");
    } else if (!strcmp(cmd, "cd")) {
        if (!valid_path(arg)) puts("invalid path");
        else if (chdir(arg)) report_error("cd");
    } else if (!strcmp(cmd, "cat")) {
        if (!valid_path(arg)) puts("invalid path");
        else cat(arg);
    } else if (!strcmp(cmd, "stat")) {
        if (!valid_path(arg)) puts("invalid path");
        else file_info(arg);
    } else if (!strcmp(cmd, "id") && !*arg) identity();
    else if (!strcmp(cmd, "mem") && !*arg) memory_info();
    else if (!strcmp(cmd, "ps") && !*arg) {
        puts("Current process only; managed process table unavailable.");
        identity();
    } else if (!strcmp(cmd, "svc") && !*arg) {
        puts("Kernel Bridge supervisor not connected; service state unknown.");
    } else if (!strcmp(cmd, "apps") && !*arg) {
        puts("Known launch aliases (installation not verified): ssh, autorun");
    } else if (!strcmp(cmd, "nro")) {
        if (!valid_path(arg)) puts("invalid path");
        else puts(nro_valid(arg) ? "NRO structural preflight passed" : "NRO missing or invalid");
    } else if (!strcmp(cmd, "run")) return run_app(arg);
    else if (!strcmp(cmd, "echo")) { safe_text(arg); putchar('\n'); }
    else if (!strcmp(cmd, "status") && !*arg) {
#ifdef __SWITCH__
        printf("EXODUS R03 | Horizon | HOS %u.%u.%u\n",
            (unsigned)HOSVER_MAJOR(hosversionGet()), (unsigned)HOSVER_MINOR(hosversionGet()),
            (unsigned)HOSVER_MICRO(hosversionGet()));
#else
        puts("EXODUS R03 | host reference");
#endif
        puts("Managed process/service provider and interactive SSH not connected. NRO handoff is not process spawn.");
    } else if (!strcmp(cmd, "clear") && !*arg) {
#ifdef __SWITCH__
        consoleClear();
#else
        puts("[clear]");
#endif
    } else puts("unknown command or arguments; use help");
    return 0;
}

#ifdef __SWITCH__
int main(void) {
    consoleInit(NULL);
    if (chdir("sdmc:/")) {
        report_error("sdmc"); consoleUpdate(NULL);
        svcSleepThread(2000000000LL); consoleExit(NULL); return 1;
    }
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    puts("EXODUS Terminal R03\nA: enter command | +: exit | help: commands");
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 buttons = padGetButtonsDown(&pad);
        if (buttons & HidNpadButton_Plus) break;
        if (buttons & HidNpadButton_A) {
            SwkbdConfig cfg;
            char line[LINE_CAP] = {0};
            Result rc = swkbdCreate(&cfg, 0);
            if (R_FAILED(rc)) { puts("keyboard unavailable"); continue; }
            swkbdConfigMakePresetDefault(&cfg);
            swkbdConfigSetHeaderText(&cfg, "EXODUS command");
            swkbdConfigSetStringLenMax(&cfg, sizeof(line)-1);
            rc = swkbdShow(&cfg, line, sizeof(line));
            swkbdClose(&cfg);
            line[sizeof(line)-1] = 0;
            if (R_SUCCEEDED(rc)) {
                fputs("> ", stdout); safe_text(line); putchar('\n');
                if (dispatch(line)) break;
            }
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
#else
int main(void) {
    char line[LINE_CAP];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '\n') && !feof(stdin)) {
            int c; while ((c = getchar()) != EOF && c != '\n') {}
            puts("line too long"); continue;
        }
        if (dispatch(line)) break;
    }
    return ferror(stdin) || ferror(stdout) ? 1 : 0;
}
#endif
