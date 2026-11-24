# Kernel Bridge R06: shell core and host demonstrator

Stacked on #34. Adds a portable command interpreter for help, ps, svc, status and exec NAME. It reads the real process table and service states; process listing is managed processes only, not system-wide. Exec resolves a trusted local manifest alias and uses the process provider without an external shell. Shell reaps only its own children; supervisor ownership remains separate.

Host demo uses the actual POSIX provider and finite /bin/true and /bin/false programs plus one explicitly labeled bootstrap test job. It waits between commands (not a PTY or asynchronous interactive desktop terminal). No claims of native execution, actual network/VFS services or privilege isolation. Commands accept complete NUL-terminated lines; console input adapter rejects overlong lines. Trusted configuration and local caller only. No service restart/stop commands until lifecycle shutdown is available. Host error/timeout exits without guaranteed cleanup; use only finite test executables.

Build host: cc -std=c11 -Wall -Wextra -Werror -pedantic tools/kernel_bridge/shell_host.c -o /tmp/exodus-shell
Run: printf 'help\nps\nsvc\nexec true\nexec false\n' | /tmp/exodus-shell

CI compiles and runs mock tests and real host demo with GCC/Clang and UBSan. Not yet observed at submission. Native keyboard input, PTY, argument support, asynchronous event loop and authorized Horizon process provider remain pending. Next: native admission/launch gates, terminal input and compositor prototype.
