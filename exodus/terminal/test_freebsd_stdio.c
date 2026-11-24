#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
FILE *fwopen(void *, int (*)(void *, const char *, int));

struct sink { char data[128]; size_t used; };
static int write_part(void *cookie, const char *buf, int len)
{
    struct sink *s = cookie;
    if (len > 3) len = 3;
    if (s->used + (size_t)len > sizeof(s->data)) { errno = ENOSPC; return -1; }
    memcpy(s->data + s->used, buf, (size_t)len);
    s->used += (size_t)len;
    return len;
}
static int fail(void *cookie, const char *buf, int len)
{
    (void)cookie; (void)buf; (void)len;
    errno = ENOSPC;
    return -1;
}
int main(void)
{
    struct sink s = {0};
    FILE *f = fwopen(&s, write_part);
    assert(f);
    assert(fprintf(f, "%s %d", "BSD", 42) == 6);
    assert(fclose(f) == 0);
    assert(s.used == 6 && !memcmp(s.data, "BSD 42", 6));
    assert(fwopen(NULL, NULL) == NULL && errno == EINVAL);
    f = fwopen(NULL, fail);
    assert(f);
    assert(fputs("error", f) >= 0);
    assert(fflush(f) == EOF && ferror(f));
    fclose(f);
    puts("fwopen contract checks passed");
    return 0;
}
