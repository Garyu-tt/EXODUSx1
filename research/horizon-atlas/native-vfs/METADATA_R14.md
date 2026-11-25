# R14 directory metadata

The VFS Broker now dispatches stat and list, with separate ACL bits, default-denied virtual methods for older providers, and validation of returned entry metadata. SdProvider implements both with libnx signatures checked against pinned fs.h blob 7f56dfbfdad6c0ae76fe959e8527ebcc3783a883. Root /sd maps to /switch/exodus/data. File metadata exposes type and size only; directory size is represented by zero, not measured. List supports 1–64 entries, at most limit+1 IPC reads, and explicit truncated indication. This is a bounded preview, not pagination or a persistent directory cursor. Invalid entry names/types, failed reads and invalid counts fail closed without partial results. All mutations remain denied.

Host test reproduction (from this directory):
```sh
for t in test_vfs test_sd_provider test_sd_metadata; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g -Itest_stubs "$t.cpp" -o "/tmp/$t" && "/tmp/$t" || exit 1
done
```
Actual Linux results: 35 + 49 + 43 = 127 assertions passed. Tested blob hashes match repository: vfs.hpp b8d7c908c9696054715764deaf013af5d23d5462; sd_provider.hpp f2bf65e7eacce748524bc56b71f269bd94a82471; test_stubs/switch.h 49e62aff96f7516a1338ee5ac5042f5f803e33cb; test_sd_provider.cpp 4c774eee545fc4f3a445888eae5b361787101099; test_vfs.cpp 6586fa06d0fd8f62dc1a63132bcc9ea0ba9abc50; test_sd_metadata.cpp fbef0524b7c06ed0217d563d50220def70b87fcf. Initial test fixture naming collision with POSIX index() fixed before upload.

No native toolchain or physical console test. Container GitHub DNS still unavailable; source transferred through connector with hash verification. Mock header is not a native ABI validation. ASCII-only names, fixed path scope, mount ownership, backend confinement, concurrent mutation, real FAT aliases and symlink semantics remain limitations. Actual console access not enabled. Next: descriptor lifecycle and POSIX shim, persistent bounded directory cursors and actual native acceptance before release.
