# R13: native SD provider candidate

SdProvider implements the existing read-only Provider interface using six libnx filesystem functions. Signatures checked against pinned libnx fs.h blob 7f56dfbfdad6c0ae76fe959e8527ebcc3783a883. Caller must keep the FS service initialized; open and close must be serialized with reads. The mount is explicit and not admitted by default.

Logical /sd/hello maps to /switch/exodus/data/hello. Directory must already exist. No directory creation or writes. The root is fixed, paths restricted to ASCII, maximum full path strictly below FS_MAX_PATH, and signed offset overflow rejected. Read size is limited to 1 MiB. File size queried before reading and EOF reads clamped. RAII closes opened files on every return. This is lexical scoping only, not proven filesystem containment; FAT aliases, concurrent mutations, actual ownership and caller permissions need target-device review. Error codes are currently collapsed to backend_error.

Host verification:
```sh
cd research/horizon-atlas/native-vfs
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g -Itest_stubs test_sd_provider.cpp -o /tmp/exodus_sd_r13
/tmp/exodus_sd_r13
```
49 assertions passed on Linux with mock libnx interfaces. Verified matching uploaded blob SHAs: sd_provider.hpp c2d7db3e62870fec239be2722e8476b05564b236; test_stubs/switch.h 8f94becca0d6fcbf0bb95eed7fdb9f4089fa9073; test_sd_provider.cpp 0c58b5cedbd542a13068873e64763f36deba5bf0. Previous R12 result: 35 host assertions, not rerun in R13.

No devkitPro toolchain present in the local container; GitHub clone could not resolve DNS. GitHub connector upload and blob verification succeeded. Real libnx compile/link, console tests, actual permissions and native Safe Mode integration remain pending. Mock header must never be used in a production build. No console access or modifications performed. Next: implement list/stat and bounded directory iteration, then device read-only acceptance.
