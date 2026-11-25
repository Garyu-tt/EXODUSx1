#define _DEFAULT_SOURCE
#include "reactor.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t alarm_seen;
static void on_alarm(int signo) { (void)signo; alarm_seen = 1; }

static void test_read_write_and_level_triggering(void)
{
    int pair[2]; char ch = 'x'; struct nx_ready out[3];
    struct nx_watch w[2] = {{-1, POLLIN, 1}, {0, POLLIN, 2}};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    w[0].fd = pair[0]; w[1].fd = pair[1];
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 0);
    assert(write(pair[1], &ch, 1) == 1);
    assert(nx_reactor_wait(w, 2, out, 2, 100) == 1);
    assert(out[0].token == 1 && (out[0].events & POLLIN));
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 1); /* level triggered */
    assert(read(pair[0], &ch, 1) == 1);
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 0);
    w[1].events = POLLOUT;
    assert(nx_reactor_wait(w, 2, out, 2, 100) == 1);
    assert(out[0].token == 2 && (out[0].events & POLLOUT));

    /* poll permits duplicate fds and reports each watch independently. */
    assert(write(pair[1], &ch, 1) == 1);
    struct nx_watch duplicates[2] = {{pair[0], POLLIN, 10}, {pair[0], POLLIN, 11}};
    assert(nx_reactor_wait(duplicates, 2, out, 2, 100) == 2);
    assert(out[0].token == 10 && out[1].token == 11);
    assert(read(pair[0], &ch, 1) == 1);

    /* A negative descriptor is ignored; events=0 is a valid poll request. */
    struct nx_watch ignored[2] = {{-1, POLLIN, 20}, {pair[0], 0, 21}};
    assert(nx_reactor_wait(ignored, 2, out, 2, 0) == 0);
    assert(close(pair[1]) == 0);
    struct nx_watch closed[2] = {{pair[0], POLLIN, 30}, {pair[1], 0, 31}};
    assert(nx_reactor_wait(closed, 2, out, 2, 100) == 2);
    assert(out[0].token == 30 && (out[0].events & POLLHUP));
    assert(out[1].token == 31 && (out[1].events & POLLNVAL));
    assert(close(pair[0]) == 0);
}

static void test_validation(void)
{
    int pair[2]; struct nx_ready out[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct nx_watch w[2] = {{pair[0], POLLIN, 10}, {pair[1], POLLIN, 20}};
    assert(nx_reactor_wait(w, 2, out, 1, 0) == -1 && errno == EINVAL);
    w[1].token = 10;
    assert(nx_reactor_wait(w, 2, out, 2, 0) == -1 && errno == EINVAL);
    w[1].token = 20; w[1].events = POLLNVAL;
    assert(nx_reactor_wait(w, 2, out, 2, 0) == -1 && errno == EINVAL);
    w[1].events = POLLIN;
    assert(nx_reactor_wait(w, 2, out, 2, -2) == -1 && errno == EINVAL);
    assert(nx_reactor_wait(w, 0, out, 2, 0) == -1 && errno == EINVAL);
    assert(nx_reactor_wait(w, 2, out, 2, 0) == 0);
    assert(close(pair[0]) == 0 && close(pair[1]) == 0);
}

static void test_priority_and_eintr(void)
{
    int pair[2]; char byte = 'x'; struct nx_ready out[1];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    struct nx_watch watch = {pair[0], POLLPRI, 42};
    /* POLLPRI is accepted even when this socket has no priority data. */
    assert(nx_reactor_wait(&watch, 1, out, 1, 0) == 0);
    assert(write(pair[1], &byte, 1) == 1);
    watch.events = POLLIN | POLLPRI;
    assert(nx_reactor_wait(&watch, 1, out, 1, 100) == 1);
    assert(out[0].events & POLLIN);
    assert(read(pair[0], &byte, 1) == 1);

    struct sigaction action; struct itimerval timer;
    memset(&action, 0, sizeof(action)); action.sa_handler = on_alarm;
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    memset(&timer, 0, sizeof(timer));
    timer.it_value.tv_usec = 20000;
    alarm_seen = 0;
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    watch.events = POLLIN;
    errno = 0;
    assert(nx_reactor_wait(&watch, 1, out, 1, 2000) == -1);
    assert(errno == EINTR && alarm_seen);
    memset(&timer, 0, sizeof(timer));
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    assert(close(pair[0]) == 0 && close(pair[1]) == 0);
}

int main(void)
{
    test_read_write_and_level_triggering();
    test_validation();
    test_priority_and_eintr();
    puts("reactor host tests passed");
    return 0;
}
