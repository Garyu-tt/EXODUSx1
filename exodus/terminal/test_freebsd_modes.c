/* Linux host probe only. Use actual libbsd mode parsing. */
#include "freebsd_parser_host.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(void)
{
    void *mode = setmode("u=rw,g=r,o=");
    assert(mode);
    assert((getmode(mode, 0777) & 0777) == 0640);
    free(mode);
    mode = setmode("0644");
    assert(mode);
    assert((getmode(mode, 0777) & 0777) == 0644);
    free(mode);
    assert(setmode("invalid-mode") == NULL);

    struct timespec a = {12, 100000000}, b = {10, 900000000}, d;
    timespecsub(&a, &b, &d);
    assert(d.tv_sec == 1 && d.tv_nsec == 200000000);
    assert(timespeccmp(&a, &b, >));
    assert(timespeccmp(&b, &a, <));
    timespecsub(&b, &a, &d);
    assert(d.tv_sec == -2 && d.tv_nsec == 800000000);
    struct timespec t;
    assert(clock_gettime(CLOCK_UPTIME, &t) == 0);
    puts("BSD host mode and monotonic time checks passed");
    return 0;
}
