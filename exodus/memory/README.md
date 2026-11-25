# EX-005 memory page compression

Use this C99 codec to store cold 4096-byte pages in smaller buffers when they contain aligned regions of zeros, runs of identical four-byte words, or full-page repeating 8- or 16-byte texels. A solid RGBA8 staging page uses 41 payload bytes. Repeated wide texels can use 9 or 17 bytes. Pass regular CPU-readable memory or a caller-owned CPU staging copy. The caller controls page lifetime, GPU transfer and synchronization.

## Build and measure

From this directory, run `make` and then `./page_probe path/to/sample.bin`.
The host probe accepts regular sample files, including empty files and links to regular files. It rejects devices, directories and named pipes.
To run the C and Python 3 tests, use `make check`. On Windows with MSYS, set `EXEEXT=.exe`. You can compile `page_codec.c` directly into a target application using its C toolchain. The codec uses only the C standard library.

The probe reads a binary sample one page at a time, reconstructs every full page, and checks each byte before reporting JSON. `sparse_pages`, `word_run_pages` and `repeated_tile_pages` count the pages encoded in each compressed format. `payload_bytes` includes format overhead and raw fallback pages. A final partial page is counted at its original size. Add `length_index_bytes` to estimate storage with a two-byte length per full page. Actual resident memory also depends on allocation strategy, offsets and staging buffers.

To run the same verification on Nintendo Switch, build the [hbmenu probe](switch/README.md). It includes generated sample pages and can scan a binary file from the SD card.

## Adaptive encoding

Call `exodus_page_encode_adaptive(page, scratch)` to try a repeated tile or word runs when their payload is smaller than the original sparse or raw representation. It uses the same buffers as the legacy encoder below. Decode adaptive output with the current `exodus_page_decode`.

Odd payload lengths select an adaptive format. The first byte identifies the encoding.

- Version `1` stores word runs. Each command byte stores the word count minus one in its low seven bits. A clear high bit means the next `count × 4` bytes are literal. A set high bit means the next four bytes repeat at least twice. Words stay in their original byte order on every target. After the 1024th word, a single zero byte pads an even payload length to odd. The decoder validates the entire stream before modifying the destination.
- Version `2` stores one 8- or 16-byte tile repeated until the page is full. Its only valid payload lengths are 9 and 17 bytes, including the version byte. The encoder checks the complete page and keeps existing 4-byte fills in version `1`. Version `2` records require the updated decoder.

## API and format

Call `exodus_page_encode(page, scratch)` with separate 4096-byte buffers. Save the returned length and copy exactly that many bytes from scratch into the chosen backing store. Release the source page only after storage succeeds. Call `exodus_page_decode(stored, length, page)` to reconstruct it. Lengths refer to the encoded payload. A successful decode requires a writable 4096-byte destination and separate source storage.

Sparse pages start with an eight-byte bitmap for 64 consecutive 64-byte blocks. Bit `i % 8` of byte `i / 8` marks an included block. Included blocks follow in ascending order. A zero page uses eight bytes. A fully populated page uses 4096 raw bytes and its length selects the raw representation. The decoder checks the sparse length against the bitmap before modifying the destination. The format provides structural validation only and has no checksum.

## Pack a group of cold pages

Link `page_store.c` and `page_codec.c` to append complete pages into a caller-owned payload buffer. Initialize an `exodus_page_store` with the buffer and its byte capacity, an array of `size_t` end offsets and its entry capacity, and zero counters. Each successful append assigns the next zero-based page index. If either buffer fills, the append returns false and keeps the existing pages intact. The caller can release the input page after a successful append.

Use `exodus_page_store_append_adaptive(&store, page)` to retain the adaptive encoder's output directly in the packed store. A solid RGBA8 page fits in a 41-byte payload buffer, and a page of identical wide texels can fit in 9 or 17 bytes. The store also needs one `size_t` end offset per page. The capacity check uses the selected encoded length. Encoding still uses a temporary 4096-byte stack buffer.

The existing `exodus_page_store_append(&store, page)` keeps the legacy sparse/raw format. Both append functions can be used in the same store, and `exodus_page_store_restore` decodes each page by its recorded length. A failed append changes neither payload bytes, end offsets nor counters.

Restore directly by index with `exodus_page_store_restore` and a separate writable page buffer. The store retains `payload_used` encoded bytes plus one `size_t` offset per page. Choose buffer capacities to fit the caller's memory budget. The caller manages the group lifetime, thread synchronization and transfers between CPU and GPU memory.

To write back a changed page, call `exodus_page_store_replace(store, index, page)` with a readable 4096-byte page. The page keeps its index and the group keeps its page count. A larger or smaller encoding shifts the following payloads and updates their offsets; all other pages retain their contents and indices. A failed capacity or index check leaves both buffers and the counters unchanged, so retain the changed source page until replacement succeeds. A smaller replacement makes capacity available for a later replacement or append.

Use `exodus_page_store_replace_adaptive` for the same replacement contract with the adaptive encoder. Legacy replacement remains unchanged. Neither function may modify the backing store of immutable deduplication groups.

Replacement uses one 4096-byte scratch buffer without heap allocation. It validates the offset array and may move the entire following payload, so its cost grows with the group size. Serialize replacement, append and restore operations. These operations manage caller-owned CPU storage; they do not update active GPU resources or provide runtime memory hooks.

## Compress successive snapshots of a page

Link `page_snapshot.c` and `page_codec.c` to retain successive versions of a 4096-byte page. Supply an immutable reference you already retain, such as a checkpointed RAM page or the previous CPU-readable RGBA staging page. The encoder compares adaptive compression of the new bytes with adaptive compression of their bytewise XOR and includes all frame overhead in the choice. Equal sizes select the standalone frame.

Run `make`, then `./page_snapshot_probe previous.bin current.bin` on two equal-length binary snapshots. It reconstructs and verifies each complete current page. The JSON reports the standalone and selected frame totals, including the raw trailing fragment. `retained_reference_bytes` counts one full reference page per relative frame. `selected_plus_reference_bytes` includes that cost when the caller retains those pages solely for this snapshot. If the checkpoint already exists independently, `selected_frame_bytes` is the incremental payload cost.

```c
uint8_t frame[EXODUS_SNAPSHOT_MAX_BYTES];
size_t size = exodus_snapshot_encode(current_page, previous_page, frame);
/* Store the first size bytes and, for a relative frame, its reference identity. */
bool restored = exodus_snapshot_decode(frame, size, previous_page, output_page);
```

Tag `0` precedes one standalone adaptive payload and allows a null reference during decode. Tag `1` precedes the reference's IEEE CRC-32 in four little-endian bytes and an adaptive XOR payload. Each frame needs at most 4097 bytes. Keep the exact reference for the lifetime of each dependent frame. The checksum detects accidental reference mismatches and has a finite collision probability. The caller controls frame integrity, reference lifetime and CPU/GPU synchronization. An in-place restore can replace the reference once no other frame needs it. Encoding uses two temporary 4096-byte arrays. Relative decoding uses one.

`make check` builds and runs the byte-for-byte snapshot tests. On the included synthetic inputs, changing one byte of a dense RAM page takes 56 bytes versus 4097 standalone. Changing a 4×4 region of a 32×32 RGBA staging page takes 134 versus 4066 bytes. An unrelated dense page takes 4097 bytes and decodes independently. These frame sizes exclude the retained reference and allocator metadata.

## Share identical cold pages

Link `page_dedup_store.c`, `page_store.c` and `page_codec.c` for an append-only group that shares identical pages. `exodus_page_dedup_store_append` assigns a logical index for every successful append; `exodus_page_dedup_store_restore` restores that index into a separate writable page. The legacy wrapper stores each unique page through the existing sparse/raw codec. Use `exodus_page_dedup_store_append_adaptive` to encode new unique pages with the adaptive codec. Both entry points may share the same group and match existing pages regardless of encoding; already stored pages are never re-encoded. For example, one zero page and two distinct repeating 8/16-byte tiles use 34 payload bytes in total, before metadata, regardless of the number of logical references. A 64-bit fingerprint filters candidates, and a full restored-byte comparison decides sharing, including when fingerprints collide.

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

## Direct word reads (Memory Lab integration)

Link `page_read.c` and include `page_read.h`. The codec-level
`exodus_page_read_word`, ordinary-store `exodus_page_store_read_word`,
and immutable dedup-store `exodus_page_dedup_store_read_word` APIs read a
zero-based 4-byte word (0–1023), in original byte order, without allocating or
materializing a 4096-byte output page. They support existing raw, sparse,
word-run and repeated-tile encodings. Word-run records are scanned and
structurally validated completely before returning a word; this is not a
constant-time lookup for all formats. The output must not overlap any backing
buffer. On failure it is left unchanged. Callers must serialize access.

This is the first integration of Memory Lab's direct-access approach with the
accepted EX-005 store, not a second dedup engine. The on-wire format and
existing append/restore/replace APIs are unchanged. Shared dedup backing
remains immutable: mutable block pools, BDI encoding, and copy-on-write
require a further integrated change. No automatic RAM/VRAM mapping or
Switch performance claim. Run `make check-direct-read` for direct-read
regression fixtures (raw, sparse, tiles, word runs, zero and shared logical
references, malformed records and indices).

### Incremental page fingerprints (v2)
The dedup store now uses XOR of position-dependent mixed 32-bit word tokens. A BDI word mutation removes the old token and inserts the new token in constant hash work, with no extra per-page metadata. All codecs use the same logical-byte fingerprint. A candidate match still requires a full decoded-byte comparison; this is not a cryptographic integrity mechanism. Rebuild any in-memory group created by the earlier FNV-1a implementation from its decoded pages before using v2; snapshot wire encodings and snapshot CRC are unchanged. Linear candidate scans, owner scans and packed payload copies remain. No Switch timing claim.

### Reusable extent pool (EX-005)
Set the optional `page_starts` array on an empty page store (including the dedup wrapper's unique store) to opt into independent extents. All arrays must be disjoint; reserve one size_t start per physical capacity slot. The existing packed layout remains the default. Pool puts reuse the old extent if possible, otherwise first-fit a gap. Unrelated payloads never move; removal shifts only physical index metadata, and the dedup wrapper updates logical references. `payload_used` counts live encoded bytes, not the backing allocation. Fragmentation can cause a failed allocation even with sufficient aggregate free bytes. No compaction, OS RAM release, free-list accelerator or kernel integration yet. The packed compact-index builder rejects pool layouts. Full extent validation and gap search are quadratic worst-case; use bounded groups. This is a first integrated page-granularity step toward R05, not its final block-slab implementation.

### Ordered extent index and bounded compaction
Set `page_order` to a separate caller-owned uint32_t array of page_capacity elements on an empty pool. With this opt-in index, full validation and first-fit are O(U), instead of O(U²); insertion/removal also update the sorted index in O(U). It costs 4 bytes per reserved physical slot plus one pointer per group. Existing unindexed pools still work. `exodus_page_store_pool_stats` reports total free bytes, largest gap, tail bytes and interior hole count. `exodus_page_dedup_store_compact` moves only whole encoded records within a caller-supplied byte budget and preserves logical references and fingerprints. Repeated calls resume naturally; a budget smaller than the next record may make no progress. The tail becomes contiguous, but the caller still owns the allocation; no actual OS page release or kernel integration. The sorted index stores record IDs, not compact physical offsets. No device performance measurements.
