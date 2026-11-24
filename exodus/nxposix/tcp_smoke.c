/* EX-016: bounded IPv4 loopback smoke test; no external network. */
#include "reactor.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

static int wait_for(int fd, short events, short expected)
{
    struct nx_watch w = {fd, events, 42};
    struct nx_ready r;
    int n = nx_reactor_wait(&w, 1, &r, 1, 2000);
    if (n == 1 && r.token == 42 && (r.events & expected)) return 0;
    if (n >= 0) errno = n == 0 ? ETIMEDOUT : EIO;
    return -1;
}

static int smoke(void)
{
    int server = -1, client = -1, peer = -1, status = 1, error = 0;
    struct sockaddr_in addr = {0};
    socklen_t len = sizeof(addr), elen = sizeof(error);
    char b = 'X', got = 0;
    const char *stage = "socket";
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) goto done;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    stage = "bind/listen";
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(server, 1) < 0 || getsockname(server, (struct sockaddr *)&addr, &len) < 0 ||
        fcntl(server, F_SETFL, O_NONBLOCK) < 0) goto done;
    stage = "connect";
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0 || fcntl(client, F_SETFL, O_NONBLOCK) < 0) goto done;
    if (connect(client, (struct sockaddr *)&addr, len) < 0 && errno != EINPROGRESS) goto done;
    if (wait_for(client, POLLOUT, POLLOUT) < 0 ||
        getsockopt(client, SOL_SOCKET, SO_ERROR, &error, &elen) < 0) goto done;
    if (error) { errno = error; goto done; }
    stage = "accept";
    if (wait_for(server, POLLIN, POLLIN) < 0) goto done;
    peer = accept(server, NULL, NULL);
    if (peer < 0 || fcntl(peer, F_SETFL, O_NONBLOCK) < 0) goto done;
    stage = "payload";
    if (send(client, &b, 1, 0) != 1 || wait_for(peer, POLLIN, POLLIN) < 0 ||
        recv(peer, &got, 1, 0) != 1) goto done;
    if (got != b) { errno = EIO; goto done; }
    stage = "EOF";
    if (shutdown(client, SHUT_WR) < 0 || wait_for(peer, POLLIN, POLLIN | POLLHUP) < 0 ||
        recv(peer, &got, 1, 0) != 0) goto done;
    status = 0;
 done:
    if (status) perror(stage);
    if (peer >= 0) close(peer);
    if (client >= 0) close(client);
    if (server >= 0) close(server);
    if (!status) puts("NX-POSIX TCP loopback: PASS");
    return status;
}

int main(void)
{
#ifdef __SWITCH__
    consoleInit(NULL);
    Result rc = socketInitializeDefault();
    int status = 1;
    if (R_SUCCEEDED(rc)) { status = smoke(); socketExit(); }
    else printf("socketInitializeDefault failed: 0x%x\n", (unsigned)rc);
    puts("Press + to exit");
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
    return smoke();
#endif
}
