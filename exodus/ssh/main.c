#include <switch.h>
#include <libssh2.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void wipe(char *p, size_t n)
{
    volatile char *v = p;
    while (n--) *v++ = 0;
}

static bool prompt(const char *label, char *out, size_t cap, bool secret)
{
    SwkbdConfig cfg;
    memset(out, 0, cap);
    if (R_FAILED(swkbdCreate(&cfg, 0))) return false;
    if (secret) swkbdConfigMakePresetPassword(&cfg);
    else swkbdConfigMakePresetDefault(&cfg);
    swkbdConfigSetHeaderText(&cfg, label);
    swkbdConfigSetStringLenMax(&cfg, cap - 1);
    Result rc = swkbdShow(&cfg, out, cap);
    swkbdClose(&cfg);
    out[cap - 1] = 0;
    return R_SUCCEEDED(rc) && out[0];
}

/* OpenSSH SHA256 fingerprint, unpadded base64. */
static void fingerprint(const unsigned char *hash, char out[51])
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    memcpy(out, "SHA256:", 7);
    unsigned acc = 0, bits = 0;
    size_t pos = 7;
    for (size_t i = 0; i < 32; ++i) {
        acc = (acc << 8) | hash[i];
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out[pos++] = alphabet[(acc >> bits) & 63];
        }
    }
    if (bits) out[pos++] = alphabet[(acc << (6 - bits)) & 63];
    out[pos] = 0;
}

static int dial(const char *host, const char *port)
{
    struct addrinfo hints = {0}, *list = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &list)) return -1;
    int fd = -1;
    for (struct addrinfo *a = list; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(fd); fd = -1; continue;
        }
        int rc = connect(fd, a->ai_addr, a->ai_addrlen);
        if (rc < 0 && errno == EINPROGRESS && fd < FD_SETSIZE) {
            fd_set wf;
            FD_ZERO(&wf); FD_SET(fd, &wf);
            struct timeval tv = {10, 0};
            int err = 0;
            socklen_t len = sizeof(err);
            rc = select(fd + 1, NULL, &wf, NULL, &tv);
            rc = rc > 0 && !getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len)
                 && !err ? 0 : -1;
        }
        if (!rc && fcntl(fd, F_SETFL, flags) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(list);
    return fd;
}

static void display(const char *buf, size_t size)
{
    /* Do not execute escape sequences from an untrusted remote process. */
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127))
            putchar(c);
        else
            putchar('.');
    }
    consoleUpdate(NULL);
}

static void run_ssh(void)
{
    char host[256], port[8], user[128], pin[64], command[512], password[256] = {0};
    int fd = -1;
    LIBSSH2_SESSION *session = NULL;
    LIBSSH2_CHANNEL *channel = NULL;
    if (!prompt("SSH host or IP", host, sizeof(host), false) ||
        !prompt("SSH port (usually 22)", port, sizeof(port), false) ||
        !prompt("SSH username", user, sizeof(user), false) ||
        !prompt("Trusted SHA256 host fingerprint", pin, sizeof(pin), false) ||
        !prompt("Remote command", command, sizeof(command), false)) goto done;
    char *end = NULL;
    long p = strtol(port, &end, 10);
    if (p < 1 || p > 65535 || *end || strlen(pin) != 50 ||
        strncmp(pin, "SHA256:", 7)) {
        puts("Invalid port or OpenSSH SHA256 fingerprint."); goto done;
    }
    puts("Connecting..."); consoleUpdate(NULL);
    fd = dial(host, port);
    if (fd < 0) { puts("TCP connection failed."); goto done; }
    session = libssh2_session_init();
    if (!session) { puts("SSH session allocation failed."); goto done; }
    libssh2_session_set_blocking(session, 1);
    libssh2_session_set_timeout(session, 15000);
    if (libssh2_session_handshake(session, fd)) {
        puts("SSH handshake failed."); goto done;
    }
    const unsigned char *hash = (const unsigned char *)
        libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    char actual[51];
    if (!hash) { puts("Server fingerprint unavailable."); goto done; }
    fingerprint(hash, actual);
    if (strcmp(actual, pin)) {
        printf("Host key mismatch. Refusing authentication.\nReceived: %s\n", actual);
        goto done;
    }
    if (!prompt("SSH password", password, sizeof(password), true)) goto done;
    int auth = libssh2_userauth_password(session, user, password);
    wipe(password, sizeof(password));
    if (auth) { puts("Authentication failed."); goto done; }
    channel = libssh2_channel_open_session(session);
    if (!channel) { puts("Cannot open SSH channel."); goto done; }
    if (libssh2_channel_handle_extended_data2(channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE) ||
        libssh2_channel_exec(channel, command) ||
        libssh2_channel_send_eof(channel)) {
        puts("Remote command setup failed."); goto done;
    }
    char buf[2048];
    while (appletMainLoop()) {
        ssize_t n = libssh2_channel_read(channel, buf, sizeof(buf));
        if (n > 0) { display(buf, (size_t)n); continue; }
        if (n == 0 && libssh2_channel_eof(channel)) {
            if (!libssh2_channel_close(channel))
                printf("\nRemote exit status: %d\n", libssh2_channel_get_exit_status(channel));
            else
                puts("\nCould not confirm remote exit status.");
            goto done;
        }
        if (n < 0) { printf("\nSSH read failed: %ld\n", (long)n); goto done; }
        svcSleepThread(10000000);
    }
done:
    wipe(password, sizeof(password));
    if (channel) libssh2_channel_free(channel);
    if (session) {
        libssh2_session_disconnect(session, "EXODUS client closed");
        libssh2_session_free(session);
    }
    if (fd >= 0) close(fd);
    consoleUpdate(NULL);
}

int main(void)
{
    consoleInit(NULL);
    if (R_FAILED(socketInitializeDefault())) {
        puts("Horizon socket initialization failed.");
        consoleUpdate(NULL);
        svcSleepThread(3000000000LL);
        consoleExit(NULL);
        return EXIT_FAILURE;
    }
    if (libssh2_init(0)) {
        socketExit(); consoleExit(NULL); return EXIT_FAILURE;
    }
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    puts("EXODUS SSH / Horizon\nA: remote command   +: exit");
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 buttons = padGetButtonsDown(&pad);
        if (buttons & HidNpadButton_Plus) break;
        if (buttons & HidNpadButton_A) {
            consoleClear();
            run_ssh();
            puts("\nA: new connection   +: exit");
        }
        consoleUpdate(NULL);
    }
    libssh2_exit();
    socketExit();
    consoleExit(NULL);
    return EXIT_SUCCESS;
}
