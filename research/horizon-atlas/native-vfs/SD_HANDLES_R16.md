# R16 persistent libnx SD handles

SdProvider now implements FileHandle and DirectoryHandle factories. Open uses a single native object for its lifetime, reads use explicit offsets, directory iteration consumes one native entry per call. A shared session owns the filesystem; closing the provider marks the session inactive, prevents further backend I/O and defers fsFsClose until all child guards are destroyed. Reopen is refused while the retired session is held. Provider destruction revokes access but does not destroy the filesystem prematurely. Existing descriptor-table cached EOF and zero-byte reads may still return without backend I/O after revocation. No write operations.

The caller must hold libnx FS service initialization until every session and handle has been destroyed; external serialization is required. No claims of thread safety, full POSIX ABI or physical FAT containment. Root scope and ASCII restrictions unchanged. Native build and real console testing remain pending.

Host test reproduction:
```sh
cd research/horizon-atlas/native-vfs
for t in test_vfs test_sd_provider test_sd_metadata test_descriptors test_sd_handles; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -g -Itest_stubs "$t.cpp" -o "/tmp/$t" && "/tmp/$t" || exit 1
done
```
Actual Linux run: 35 + 49 + 43 + 212 + 149 = 488 checks passed. All target interfaces mocked; not native ABI validation. Verified matching uploaded blob hashes: sd_provider.hpp ca30d8c1a53c38d7d363dae2e113aaab9b38da4f; test_sd_handles.cpp bbf0739efd6cd32238fcccc15cfe098947265350. Container GitHub DNS unavailable; transferred via connector. No console changes. Next: RAM FS/RomFS providers, then toolchain build and read-only device acceptance.
