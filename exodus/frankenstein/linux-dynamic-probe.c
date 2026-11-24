/* Real dynamically linked Linux AArch64 program. No Steam binaries. */
#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <gnu/libc-version.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
static void *worker(void *p) { return p; }
int main(void) {
    struct utsname name;
    struct timespec ts;
    pthread_t thread;
    int token = 42;
    void *result = NULL;
    printf("TUMOR_GLIBC_START version=%s\n", gnu_get_libc_version());
    fflush(stdout);
    if (uname(&name) || getpid() <= 0 || clock_gettime(CLOCK_MONOTONIC, &ts))
        return 1;
    printf("TUMOR_GLIBC_SYSCALLS_OK sys=%s release=%s machine=%s\n",
           name.sysname, name.release, name.machine);
    fflush(stdout);
    void *lib = dlopen("libc.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!lib || !dlsym(lib, "getpid")) return 2;
    dlclose(lib);
    puts("TUMOR_GLIBC_DLOPEN_OK");
    fflush(stdout);
    if (pthread_create(&thread, NULL, worker, &token)) return 3;
    if (pthread_join(thread, &result) || result != &token) return 4;
    puts("TUMOR_GLIBC_PTHREAD_OK");
    return 0;
}
