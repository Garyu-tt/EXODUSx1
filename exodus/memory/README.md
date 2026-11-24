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

Link `page_dedup_store.c`, `page_store.c` and `page_codec.c` for an append-only group that shares identical pages. `exodus_page_dedup_store_append` assigns a logical index for every successful append; `exodus_page_dedup_store_restore` restores that index into a separate writable page. The wrapper stores each unique page through the existing sparse/raw codec. A 64-bit fingerprint filters candidates, and a full restored-byte comparison decides sharing, including when fingerprints collide.

Initialize the wrapper's `unique` store as above, then supply `fingerprints` with `unique.page_capacity` elements of `uint64_t`, `page_refs` with `page_capacity` elements of `size_t`, and zero both page counts and `unique.payload_used`. All four backing buffers, source pages and restore outputs must be separate. For example, two identical raw pages fit in this group:

```c
uint8_t payload[EXODUS_PAGE_BYTES];
size_t ends[1], references[2];
uint64_t fingerprints[1];
struct exodus_page_dedup_store group = {
    .unique = { .payload = payload, .payload_capacity = sizeof(payload),
                .page_ends = ends, .page_capacity = 1 },
    .fingerprints = fingerprints, .page_refs = references, .page_capacity = 2
};
/* After checking each append's return value, release or reuse its source page.
 * Restore logical indices 0 and 1 into separate writable page buffers. */
```

A duplicate can be appended when the physical page or payload buffer is full, provided a logical index remains. Capacity failures leave all buffers and counters unchanged. The wrapper exclusively owns its physical store: callers must not append to it, replace shared pages, edit its metadata or free its buffers independently. Calls must be serialized. Editing a restored copy does not affect either logical page. Updating an existing logical page and copy-on-write are not provided by this API; keep a changed copy outside the group or append it at a new logical index.

For `U` unique pages and `L` logical pages, active storage is `unique.payload_used + U * (sizeof(size_t) + sizeof(uint64_t)) + L * sizeof(size_t)`, plus the wrapper descriptor. Reserved storage instead uses the caller's payload, unique-index, fingerprint and logical-index **capacities**. The host test's two equal raw pages use 4096 payload bytes instead of 8192; on a 64-bit host the complete reserved backing, metadata and descriptor total 4208 bytes instead of the ordinary store's 8256. These totals exclude caller staging and temporary append/restore workspace, which can require up to two additional 4096-byte page buffers during append. Reducing used bytes inside an existing allocation does not release RAM.

The fingerprint index uses a bounded linear scan: append checks up to `U` fingerprints and restores only matching candidates. Building an all-unique group can therefore require quadratic index work; use bounded cold-page groups and measure the workload. This is CPU-side storage, without GPU residency changes, automatic write-back, or a physical Switch performance claim. `make check` covers sharing, collision checks, capacity failure preservation and mixed-page round trips.
