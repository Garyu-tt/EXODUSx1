# EX-005 memory page compression

Use this C99 codec to store cold 4096-byte pages in smaller buffers when they contain aligned regions of zeros or runs of identical four-byte words. A solid RGBA staging page uses 41 payload bytes. Pass regular CPU-readable memory or a caller-owned CPU staging copy. The caller controls page lifetime, GPU transfer and synchronization.

## Build and measure

From this directory, run `make` and then `./page_probe path/to/sample.bin`. To run the C and Python 3 tests, use `make check`. On Windows with MSYS, set `EXEEXT=.exe`. You can compile `page_codec.c` directly into a target application using its C toolchain. The codec uses only the C standard library.

The probe reads a binary sample one page at a time, reconstructs every full page, and checks each byte before reporting JSON. `sparse_pages` and `word_run_pages` count the pages encoded in each compressed format. `payload_bytes` includes format overhead and raw fallback pages. A final partial page is counted at its original size. Add `length_index_bytes` to estimate storage with a two-byte length per full page. Actual resident memory also depends on allocation strategy, offsets and staging buffers.

To run the same verification on Nintendo Switch, build the [hbmenu probe](switch/README.md). It includes generated sample pages and can scan a binary file from the SD card.

## Adaptive encoding

Call `exodus_page_encode_adaptive(page, scratch)` to select word runs when their payload is smaller than the original sparse or raw representation. It uses the same buffers as the legacy encoder below. Decode adaptive output with the current `exodus_page_decode`.

Odd payload lengths select the word-run format. The first byte is version `1`. Each command byte stores the word count minus one in its low seven bits. A clear high bit means the next `count × 4` bytes are literal. A set high bit means the next four bytes repeat at least twice. Words stay in their original byte order on every target. After the 1024th word, a single zero byte pads an even payload length to odd. The decoder validates the entire stream before modifying the destination.

## API and format

Call `exodus_page_encode(page, scratch)` with separate 4096-byte buffers. Save the returned length and copy exactly that many bytes from scratch into the chosen backing store. Release the source page only after storage succeeds. Call `exodus_page_decode(stored, length, page)` to reconstruct it. Lengths refer to the encoded payload. A successful decode requires a writable 4096-byte destination and separate source storage.

Sparse pages start with an eight-byte bitmap for 64 consecutive 64-byte blocks. Bit `i % 8` of byte `i / 8` marks an included block. Included blocks follow in ascending order. A zero page uses eight bytes. A fully populated page uses 4096 raw bytes and its length selects the raw representation. The decoder checks the sparse length against the bitmap before modifying the destination. The format provides structural validation only and has no checksum.

## Pack a group of cold pages

Link `page_store.c` and `page_codec.c` to append complete pages into a caller-owned payload buffer. Initialize an `exodus_page_store` with the buffer and its byte capacity, an array of `size_t` end offsets and its entry capacity, and zero counters. Each successful append assigns the next zero-based page index. If either buffer fills, the append returns false and keeps the existing pages intact. The caller can release the input page after a successful append.

Restore directly by index with `exodus_page_store_restore` and a separate writable page buffer. The store retains `payload_used` encoded bytes plus one `size_t` offset per page. Choose buffer capacities to fit the caller's memory budget. The caller manages the group lifetime, thread synchronization and transfers between CPU and GPU memory.

## Share identical cold pages

Link `page_dedup_store.c` with the existing store and codec to build an immutable group containing one encoded copy of each distinct page. Initialize an `exodus_page_dedup_store` with a zero-count `unique_pages` store, a `uint64_t` fingerprint array with `unique_pages.page_capacity` slots, a separate `size_t` reference array with `page_capacity` slots, and zero logical `page_count`. These are caller-owned memory buffers, not a serialized file format.

Call `exodus_page_dedup_store_append(&store, page)` for each 4096-byte input. Every successful append assigns a stable logical index, including duplicates. The wrapper uses a fingerprint to find possible matches, then restores and compares all 4096 bytes before sharing storage. A collision cannot make distinct page contents share an entry. New unique pages use the existing sparse/raw encoder; the codec and ordinary page-store APIs are unchanged. Identical pages can still be appended when the unique-page index or payload is full, provided a logical reference slot remains. Any failed append preserves all backing bytes, references, fingerprints and counters.

Restore a logical page with `exodus_page_dedup_store_restore(&store, index, destination)`. The destination is an independent writable copy. Retain and serialize access to all backing buffers for the group's lifetime. Do not modify the embedded `unique_pages` store, fingerprints, references or encoded bytes directly: changing a shared page would change multiple logical pages. Updates, deletion and copy-on-write are not provided by this immutable wrapper; retire or rebuild the group when its contents must change.

For `U` unique-page slots and `L` logical-page slots, reserved backing memory is `payload_capacity + U * (sizeof(size_t) + sizeof(uint64_t)) + L * sizeof(size_t)`, plus the store structure. On a platform with an eight-byte `size_t`, two identical dense pages fit in a 4096-byte payload, one unique slot and two logical slots: 4128 bytes of backing buffers plus the structure. Two ordinary dense pages need 8192 payload bytes and two end offsets. Larger reserved capacities still consume their allocated space even when `payload_used` is smaller. Encoding and collision comparisons also use temporary full-page CPU buffers, and the caller supplies each input/output page.

Append hashes one input page and scans the `U` existing fingerprints; each matching fingerprint requires one decode and full-page comparison. This bounded linear lookup trades additional metadata and CPU work for deduplication in small cold-page groups. It performs no dynamic allocation, GPU synchronization or live RAM/VRAM residency management. Measure against the intended staging workload before choosing group sizes.
