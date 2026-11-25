# EX-005 memory page compression

Use this C99 codec to store cold 4096-byte pages in smaller buffers when they contain aligned regions of zeros, runs of identical four-byte words, or full-page repeating 8- or 16-byte texels. A solid RGBA8 staging page uses 41 payload bytes. Repeated wide texels can use 9 or 17 bytes. Pass regular CPU-readable memory or a caller-owned CPU staging copy. The caller controls page lifetime, GPU transfer and synchronization.

## Build and measure

From this directory, run `make` and then `./page_probe path/to/sample.bin`. To run the C and Python 3 tests, use `make check`. On Windows with MSYS, set `EXEEXT=.exe`. You can compile `page_codec.c` directly into a target application using its C toolchain. The codec uses only the C standard library.

The probe reads a binary sample one page at a time, reconstructs every full page, and checks each byte before reporting JSON. `sparse_pages`, `word_run_pages` and `repeated_tile_pages` count the pages encoded in each compressed format. `payload_bytes` includes format overhead and raw fallback pages. A final partial page is counted at its original size. Add `length_index_bytes` to estimate storage with a two-byte length per full page. Actual resident memory also depends on allocation strategy, offsets and staging buffers.

To run the same verification on Nintendo Switch, build the [hbmenu probe](switch/README.md). It includes generated sample pages and can scan a binary file from the SD card.

## Adaptive encoding

Call `exodus_page_encode_adaptive(page, scratch)` to try a repeated tile or word runs when their payload is smaller than the original sparse or raw representation. It uses the same buffers as the legacy encoder below. Decode adaptive output with the current `exodus_page_decode`.

Odd payload lengths select an adaptive format. The first byte identifies the encoding and all payload bytes stay in their original order.

- Version `1` stores word runs. Each command byte stores the word count minus one in its low seven bits. A clear high bit means the next `count × 4` bytes are literal. A set high bit means the next four bytes repeat at least twice. After the 1024th word, a single zero byte pads an even payload length to odd. The decoder validates the entire stream before modifying the destination.
- Version `2` stores one 8- or 16-byte tile repeated until the page is full. Its only valid payload lengths are 9 and 17 bytes, including the version byte. The encoder checks the complete page before selecting this format and keeps existing 4-byte fills in version `1`. Version `2` records require an updated decoder.

## API and format

Call `exodus_page_encode(page, scratch)` with separate 4096-byte buffers. Save the returned length and copy exactly that many bytes from scratch into the chosen backing store. Release the source page only after storage succeeds. Call `exodus_page_decode(stored, length, page)` to reconstruct it. Lengths refer to the encoded payload. A successful decode requires a writable 4096-byte destination and separate source storage.

Sparse pages start with an eight-byte bitmap for 64 consecutive 64-byte blocks. Bit `i % 8` of byte `i / 8` marks an included block. Included blocks follow in ascending order. A zero page uses eight bytes. A fully populated page uses 4096 raw bytes and its length selects the raw representation. The decoder checks the sparse length against the bitmap before modifying the destination. The format provides structural validation only and has no checksum.

## Pack a group of cold pages

Link `page_store.c` and `page_codec.c` to append complete pages into a caller-owned payload buffer. Initialize an `exodus_page_store` with the buffer and its byte capacity, an array of `size_t` end offsets and its entry capacity, and zero counters. Each successful append assigns the next zero-based page index. If either buffer fills, the append returns false and keeps the existing pages intact. The caller can release the input page after a successful append.

Restore directly by index with `exodus_page_store_restore` and a separate writable page buffer. The store retains `payload_used` encoded bytes plus one `size_t` offset per page. Choose buffer capacities to fit the caller's memory budget. The caller manages the group lifetime, thread synchronization and transfers between CPU and GPU memory.
