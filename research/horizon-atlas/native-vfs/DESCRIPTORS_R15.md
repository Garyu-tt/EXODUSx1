# R15 persistent descriptor table

The internal C++17 table implements POSIX-style open/read/close/lseek/fstat/opendir/readdir semantics for providers that implement the new persistent FileHandle and DirectoryHandle contracts. It does not interpose libc, expose a POSIX ABI, or reopen a file by name on each read. The existing SD provider has not yet implemented these handle factories and therefore returns denied for descriptor opens; its older path-based operations remain unchanged. Mock provider tested only. No automatic admission.

Each table owns at most 64 simultaneous handles, using non-recycled IDs from 3 until int exhaustion. Independent signed 64-bit offsets, 1 MiB read cap, EOF, checked seek arithmetic, distinct mount permissions and positive errno-style results. fstat/SEEK_END require stat permission. Sequential directory cursors have no 64-entry preview limit; EOF and backend failure are sticky. Destruction closes owned handles through RAII. All mutations denied. Table and broker/provider lifetimes must be externally serialized; no concurrent access, revocation, dup, directory fstat, global errno or true POSIX flags. Allocation failure behavior and native result-to-errno mapping still need implementation.

Actual Linux GCC host tests:
```sh
for t in test_vfs test_sd_provider test_sd_metadata test_descriptors; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -g -Itest_stubs "$t.cpp" -o "/tmp/$t" && "/tmp/$t" || exit 1
done
```
Results: 35 + 49 + 43 + 212 = 339 checks passed. Directory fixture yielded 100 entries. All three modified/new files verified after upload against locally tested Git blob hashes: vfs.hpp c2f2b5c0f56b84258701fb23b36f9eb751dd8ae1; descriptor_table.hpp b81369d127cfa919fd3d67af32a1348c3ef967d4; test_descriptors.cpp de37f0ad39296aaccc174cc7cee011d3fc9355e4.

Next: implement persistent libnx FsFile/FsDir handles with safe mount lifetime, then a native toolchain build and owned SD read-only device smoke test before shell integration. No console access or modification in R15.
