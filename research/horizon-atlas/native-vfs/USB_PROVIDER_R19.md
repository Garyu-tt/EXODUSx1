# R19 USB provider boundary

UsbProvider integrates UsbGate leases with existing Broker and DescriptorTable. File/dir handles retain backend lifetime and check volume identity and readonly generation before backend operations. A bounded read and directory preview are implemented. No physical backend is implemented; the test backend uses MemoryProvider. The wrapper assumes serialized access and an authoritative generation. Backend operations and destructors must be safe under physical removal, with atomic generation checks and lifetime protection inside the native backend. The separate volume() check is not atomic with I/O. DescriptorTable cached EOF and zero-byte reads may return without touching the backend. No raw device paths or mutations are exposed.

## Upstream detach blocker
ITotalJustice/libusbhsfs d0a973e:
- source/fatfs/ff_dev.c blob 14d3a68027a3d5eab122d0f91225c75b0ec2563b lines 32–43 dereference r->deviceData and the filesystem/LUN contexts before acquiring the manager-validated drive lock. File close/read and directory next/close use those macros.
- source/usbhsfs_manager.c blob 8b21e49da44c47dea1e41b019cbd2c55c34d7b39 lines 478–501 check a drive pointer and lock it; lines 965–975 destroy and free drive context.
- source/usbhsfs_mount.c blob c42bce395d222a45bb3b9a91361fcfc7bed4287e lines 294–318 unregister and free devoptab interface; lines 964–977 unmount and free FATFS.

This suggests a stale-pointer lifetime risk, not a reproduced vulnerability. Do not admit an unmodified native devoptab backend merely because a wrapper checks the current mount name. Next: donor patch/fork design with stable generation-bound control blocks, serialized acquire/revoke, safe deferred destruction of open descriptors, and hot-unplug tests including close after detach. Native acceptance and write-protected owned media remain required.

## Verification
Actual local Linux run: 28 assertions passed under GCC ASan/UBSan.
```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -g test_usb_provider.cpp -o /tmp/test_usb_provider
/tmp/test_usb_provider
```
Uploaded blobs verified equal to tested files: usb_provider.hpp e4842d7a101449e808e877798950980d6bd69c89; test_usb_provider.cpp 85aacdbca7c699627e02bcecbdf5c8339b4b94db. Previous 690 assertions not rerun. No native build or console access.
