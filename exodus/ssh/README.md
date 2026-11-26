# EXODUS SSH 0.1

Native Horizon homebrew SSH client (libnx sockets, libssh2 protocol). No Wine, Linux VM or local POSIX shell required. The first slice executes one remote command per connection, not an interactive PTY.

## Build

Install devkitA64, libnx, switch-tools and Switch portlibs for libssh2, its crypto backend and zlib. For a devkitPro distribution offering these packages:

    dkp-pacman -S switch-dev switch-libssh2 switch-mbedtls switch-zlib
    make -C exodus/ssh

The Makefile defaults to the mbedTLS backend; override SSH_LIBS if the installed libssh2 uses another backend. Package availability and actual linkage must be verified in the target SDK. Copy exodus/ssh/exodus-ssh.nro to /switch/exodus-ssh/ on SD and launch via homebrew in application mode.

## Use

Press A, enter host, port, user, trusted OpenSSH SHA256 fingerprint and command. Obtain the fingerprint through an independently authenticated channel, e.g. on the server console:

    ssh-keygen -lf /etc/ssh/ssh_host_ecdsa_key.pub -E sha256

Use the fingerprint for the host key actually negotiated (algorithm depends on both peers). Never copy the fingerprint from a mismatch warning as a substitute for independently verifying it. Authentication is refused before password input if the pin does not match. The password is entered with the masked Horizon keyboard and wiped from the application buffer; it is not saved. Remote stdout/stderr are merged; non-ASCII and control sequences are filtered in this initial console. Remote exit status is reported when confirmed. Plus exits the launcher between commands.

## Acceptance gates (not yet executed)

- Cross-build and package NRO with the actual Switch SDK; verify target crypto backend.
- On an authorized test SSH server, test correct pin and wrong pin, wrong password, non-default port, unreachable server, stdout/stderr, exit 7, output larger than the read buffer, and disconnection/timeout.
- Confirm no password is persisted; verify repeated connections and cleanup on real hardware.
- Confirm negotiated modern host-key and key-exchange algorithms; do not enable legacy algorithms just to make a connection work.
- Blocking operations have per-operation timeout, not an overall session deadline; DNS and app lifecycle behavior need hardware testing.

## Follow-on architecture

Expose an SSH transport provider to the future EXODUS terminal with interactive PTY, nonblocking bidirectional I/O, cancellation, terminal resize and safe VT parser. Add public-key auth, encrypted key storage, host-specific known_hosts, SFTP and optional SSH daemon with explicit opt-in, restricted command capabilities and public-key-only authentication. The daemon is a separate component, disabled by default. No SSH server or interactive terminal is included in 0.1.
