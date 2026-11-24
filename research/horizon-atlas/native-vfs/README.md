# R12 C++ VFS broker prototype

Independent C++17 implementation, not copied from Kefir. Host build and run:
```sh
cd research/horizon-atlas/native-vfs
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g test_vfs.cpp -o /tmp/exodus-vfs-test
/tmp/exodus-vfs-test
```
35 checks passed in a Linux host build with AddressSanitizer and UndefinedBehaviorSanitizer. Tested file blob SHAs: vfs.hpp 37147e207619d2f492b96346a31a412dfbfd7a29; test_vfs.cpp 6586fa06d0fd8f62dc1a63132bcc9ea0ba9abc50. Both verified after GitHub upload.

Only read dispatch is implemented; list/stat are routing declarations, not backend operations. Mutation and unknown operations denied. Fixture providers are not real mounts. ASCII-only path policy intentionally rejects non-ASCII and trailing separators until encoding and alias semantics are reviewed. A 1 MiB maximum per read is a design choice, not measured hardware limit. Returned route views borrow caller path and broker mount storage; provider lifetime and backend buffer safety are caller/provider contracts. This prototype is not a filesystem sandbox: physical path containment, symlinks, race resistance, provider ownership, caller identities and thread safety still require implementation. No native Switch build, CI or device test performed.

Default-branch code search returned no relevant files and the expected VFS/security build files were absent from the research branch. This is not evidence that no VFS exists on other branches. Keep the prototype isolated until the open shell/kernel/security branches are reconciled. Next: owned native SD read-only provider, list/stat, descriptor lifetime, and Safe Mode integration, with separate native acceptance tests.
