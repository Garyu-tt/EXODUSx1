/* Portable TCP loopback fixture. No external network or AF_UNIX required. */
#include "reactor.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL line %d: %s (errno=%d)\n", __LINE__, #expr, errno); \
    goto done; } } while (0)

static int run_tcp(void)
{
    int listener = -1, client = -1, peer = -1, status = 1;
    struct sockaddr_in addr = {0};
    socklen_t len = sizeof(addr);
    struct nx_ready ready[1];
    struct nx_watch watch = {-1, POLLIN, 42};
    char byte = 'X', got = 0;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(listener >= 0);
    CHECK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(getsockname(listener, (struct sockaddr *)&addr, &len) == 0);
    CHECK(addr.sin_port != 0);
    CHECK(listen(listener, 1) == 0);
    client = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(client >= 0);
    CHECK(connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    watch.fd = listener;
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 1000) == 1);
    CHECK(ready[0].token == 42 && (ready[0].events & POLLIN));
    peer = accept(listener, NULL, NULL);
    CHECK(peer >= 0);
    watch.fd = peer;
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 0) == 0);
    CHECK(send(client, &byte, 1, 0) == 1);
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 1000) == 1);
    CHECK(ready[0].token == 42 && (ready[0].events & POLLIN));
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 0) == 1);
    CHECK(recv(peer, &got, 1, 0) == 1 && got == byte);
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 10) == 0);
    watch.events = POLLOUT;
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 1000) == 1);
    CHECK(ready[0].events & POLLOUT);
    CHECK(shutdown(client, SHUT_WR) == 0);
    watch.events = POLLIN;
    CHECK(nx_reactor_wait(&watch, 1, ready, 1, 1000) == 1);
    CHECK(ready[0].events & (POLLIN | POLLHUP));
    CHECK(recv(peer, &got, 1, 0) == 0);
    status = 0;
    puts("TCP loopback reactor smoke passed");
done:
    if (peer >= 0) close(peer);
    if (client >= 0) close(client);
    if (listener >= 0) close(listener);
    return status;
}

int main(void)
{
#ifdef __SWITCH__
    consoleInit(NULL);
    Result rc = socketInitializeDefault();
    int status = 1;
    if (R_SUCCEEDED(rc)) {
        status = run_tcp();
        socketExit();
    } else {
        printf("socketInitializeDefault failed: 0x%08x\n", (unsigned)rc);
    }
    puts("Press PLUS to exit");
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return status;
#else
    return run_tcp();
#endif
}
