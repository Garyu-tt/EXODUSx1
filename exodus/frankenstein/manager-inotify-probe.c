#include <sys/inotify.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    int fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (fd < 0) return 2;
    if (inotify_add_watch(fd, argv[1], IN_CREATE) < 0) return 3;
    int dir = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) return 4;
    int created = openat(dir, "probe-event", O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (created < 0) return 5;
    close(created);
    struct pollfd p = { .fd = fd, .events = POLLIN };
    if (poll(&p, 1, 5000) != 1 || !(p.revents & POLLIN)) return 6;
    char buffer[4096];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    struct inotify_event event;
    if (n < (ssize_t)sizeof(event)) return 7;
    memcpy(&event, buffer, sizeof(event));
    if (!(event.mask & IN_CREATE) || event.len < sizeof("probe-event") ||
        event.len > (size_t)n - sizeof(event) ||
        memcmp(buffer + sizeof(event), "probe-event", sizeof("probe-event"))) return 8;
    close(dir);
    close(fd);
    return 0;
}
