# Hardware Atlas R04 — owned buffer ledger and interop evidence

Adds native-hardware/buffer_ledger.hpp and test_buffer_ledger.cpp. The ledger is a C++17 accounting and transition model, not an allocator, driver or fence implementation. One mutex serializes budget, slot and transition operations. Monotonic IDs are not reused. Only trusted internal callers may attest ownership or completion events. Release before unmapping and cache restoration is denied; untouched allocations can be cancelled. Uncertain buffers retain their reservation in quarantine with no recovery API. At the quarantine threshold new reservations and submissions stop; existing mappings can still be cleaned up. The threshold is a stop trigger, not a strict maximum quarantine size (existing reservations may fail later); overall reserved bytes remain bounded. No actual pages or per-engine budgets, no ownership enforcement, no driver event authentication. Do not use as a production memory safety boundary.

Host test:
```sh
cd research/horizon-atlas/native-hardware
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -g test_buffer_ledger.cpp -o /tmp/exodus-buffer-ledger
/tmp/exodus-buffer-ledger
```
Actual local Linux result: 48 checks passed, including 32 competing reservations and 16 competing releases. Uploaded blob hashes verified equal: buffer_ledger.hpp 8c3f1c33a532afb38161f1743b94c6e7e9791160; test_buffer_ledger.cpp 8b988eda0c36fa1ca94080edb54d5113b73f61f2. No ThreadSanitizer or native tests; historical R03 25 JS checks not rerun.

Interop JSON records six candidate paths. Pinned libnx map.c shows page alignment and unpropagated cache-attribute and free results. A void nvMapClose cannot establish successful release; a checked native adapter is needed. Deko3d storage pointer and imported fence support are evidence for investigation, not proof of compatible cross-engine allocation ownership. Verify formats, strides, planes, layout, cache, producer and consumer fences, and copy counts. No raw register access, foreign buffers or physical device operations. Next: checked native buffer adapter and multi-consumer state model, bounded fence waits and synthetic owned-frame pipeline with explicit copy fallback.
