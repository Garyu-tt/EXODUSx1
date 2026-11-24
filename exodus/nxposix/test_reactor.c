#include "reactor.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    int pair[2];
    char ch = 'x';
    struct nx_ready out[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct nx_watch w[2] = {{pair[0], POLLIN, 10}, {pair[1], POLLIN, 20}};
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 0);
    assert(write(pair[1], &ch, 1) == 1);
    assert(nx_reactor_wait(w, 2, out, 2, 100) == 1);
    assert(out[0].token == 10 && (out[0].events & POLLIN));
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 1); /* level triggered */
    assert(read(pair[0], &ch, 1) == 1);
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 0);
    w[1].events = POLLOUT;
    assert(nx_reactor_wait(w, 2, out, 2, 100) == 1);
    assert(out[0].token == 20 && (out[0].events & POLLOUT));
    assert(nx_reactor_wait(w, 2, out, 1, 0) == -1 && errno == EINVAL);
    w[1].token = 10;
    assert(nx_reactor_wait(w, 2, out, 2, 0) == -1 && errno == EINVAL);
    w[1].token = 20; w[1].fd = pair[0];
    assert(nx_reactor_wait(w, 2, out, 2, 0) == -1 && errno == EINVAL);
    w[1].fd = pair[1]; w[1].events = 0;
    assert(nx_reactor_wait(w, 2, out, 2, 0) == -1 && errno == EINVAL);
    w[1].events = POLLIN;
    assert(nx_reactor_wait(w, 2, out, 2, -2) == -1 && errno == EINVAL);
    assert(close(pair[1]) == 0);
    assert(nx_reactor_wait(w, 2, out, 2, 100) == 2);
    assert(out[0].token == 10 && (out[0].events & POLLHUP));
    assert(out[1].token == 20 && (out[1].events & POLLNVAL));
    assert(close(pair[0]) == 0);
    puts("reactor host tests passed");
    return 0;
}
