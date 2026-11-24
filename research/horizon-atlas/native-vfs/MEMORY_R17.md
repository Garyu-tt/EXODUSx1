# R17 RAM and RomFS snapshots

MemoryProvider implements immutable in-memory files, implicit parent directories, metadata and persistent file/directory handles. A trusted caller seeds the snapshot; broker mutations remain denied. A snapshot is limited to 4 MiB logical file payload and 512 total nodes including root and implicit directories. These are design limits, not measured Switch limits or total heap bounds (metadata, vector capacity, transient input, concurrent snapshots and allocations are additional). Directory handles retain a shared immutable snapshot; this intentionally preserves data after provider destruction, not a revocable mount. Broker/provider lifetime contract still applies to new opens. No writable tmpfs or general POSIX ABI.

RomFS snapshot loader reads an explicit trusted manifest of application resources under romfs:/exodus/ using read-only stdio, with the same payload/node quotas. Caller must initialize the real libnx RomFS mount; this code is not a direct RomFS parser, mount initializer or streaming provider. Host fixture uses a literal local directory named romfs:, not actual libnx. Native build, real RomFS and physical device testing pending. Do not use untrusted host directory/symlink fixtures as a containment boundary. No SD or console modification.

Reproduce from native-vfs directory (Linux):
```sh
set -eu
src="$PWD"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/romfs:/exodus/icons"
printf test > "$work/romfs:/exodus/config.txt"
: > "$work/romfs:/exodus/icons/empty"
truncate -s 4194305 "$work/romfs:/exodus/large"
for t in test_vfs test_sd_provider test_sd_metadata test_descriptors test_sd_handles test_memory; do
  g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -g -I"$src/test_stubs" "$src/$t.cpp" -o "$work/$t"
  (cd "$work" && "./$t")
done
```
Actual Linux results: 35 + 49 + 43 + 212 + 149 + 155 = 643 assertions. Initial GCC warning about attributes on fclose function-pointer template argument fixed via a lambda deleter, then memory suite passed. Verified uploaded Git blobs match locally tested code: memory_provider.hpp 26ee5cc5631a3729c9f652504c87b6120ff47562; romfs_snapshot.hpp 16a7506c07417268ab29bf7af784a0fb7c733c93; test_memory.cpp 954554bcd866711905e575fd16d6b36746b503ba. Host-only, native toolchain absent. Node quota boundary implemented but not separately stress-tested in this suite. Allocation failures and memory budget across multiple snapshots remain open.

Next: USB storage donor audit and read-only provider interface; prioritize native devkitPro build and explicit owned-media acceptance before enabling any physical provider.
