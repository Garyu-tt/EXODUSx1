# EX-005 sparse memory pages

Use this C99 codec to store a cold 4096-byte page in a smaller buffer when it contains aligned runs of zeros. Pass regular CPU-readable memory or a caller-owned CPU staging copy. The caller controls page lifetime, GPU transfer and synchronization.

## Build and measure

From this directory, run `make` and then `./page_probe path/to/sample.bin`. To run the C and Python 3 tests, use `make check`. On Windows with MSYS, set `EXEEXT=.exe`. You can compile `page_codec.c` directly into a target application using its C toolchain. The codec uses only the C standard library.

The probe reads a binary sample one page at a time, reconstructs every full page, and checks each byte before reporting JSON. A final partial page is counted at its original size. `payload_bytes` includes the masks and raw fallback pages. Add `length_index_bytes` to estimate storage with a two-byte length per full page. Actual resident memory also depends on allocation strategy, offsets and staging buffers. The probe measures a file offline. It does not install a runtime hook or measure GPU residency.

To run the same verification on Nintendo Switch, build the [hbmenu probe](switch/README.md). It includes generated sample pages and can scan a binary file from the SD card.

## API and format

Call `exodus_page_encode(page, scratch)` with separate 4096-byte buffers. Save the returned length and copy exactly that many bytes from scratch into the chosen backing store. Release the source page only after storage succeeds. Call `exodus_page_decode(stored, length, page)` to reconstruct it. Lengths refer to the encoded payload. A successful decode requires a writable 4096-byte destination and separate source storage.

Sparse pages start with an eight-byte bitmap for 64 consecutive 64-byte blocks. Bit `i % 8` of byte `i / 8` marks an included block. Included blocks follow in ascending order. A zero page uses eight bytes. A fully populated page uses 4096 raw bytes and its length selects the raw representation. The decoder checks the sparse length against the bitmap before modifying the destination. The format provides structural validation only and has no checksum.
