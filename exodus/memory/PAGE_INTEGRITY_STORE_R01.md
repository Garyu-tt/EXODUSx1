# EX-005 page integrity store R01

`page_integrity_store` is an opt-in C99 wrapper around the existing packed cold
page store. It retains the codec's sparse, raw and adaptive representations and
adds one caller-owned IEEE CRC-32 value per logical 4 KiB page.

The ordinary codec intentionally provides structural validation only. A bit
flip in raw data, or in the data portion of a structurally valid compressed
record, can therefore decode successfully to different bytes. The integrity
wrapper computes the checksum before append and restores into a 4 KiB scratch
page. It copies to the caller's destination only after the complete decoded
page matches the retained checksum. Invalid metadata, malformed records and
checksum mismatches leave the destination untouched.

Initialize `struct exodus_page_integrity_store` with the same caller-owned
payload and end-offset buffers used by `exodus_page_store`, plus a separate
`uint32_t` checksum array. The checksum capacity may independently limit the
logical page count. Both append variants are failure-atomic across payload,
offset and checksum metadata. The wrapper also rejects inconsistent end-offset
metadata before append or restore.

Use `exodus_page_integrity_store_append()` for the legacy sparse/raw encoder or
`exodus_page_integrity_store_append_adaptive()` for the smallest current codec
representation. Use `exodus_page_integrity_store_restore()` with a separate
writable 4 KiB destination.

## Boundary and non-claims

- Metadata overhead is one `uint32_t` checksum in addition to the existing
  `size_t` end offset per logical page. Restore uses a temporary 4 KiB stack
  page so damaged bytes never reach the caller's output.
- Payload, offsets and checksums form one immutable group after construction.
  The caller owns capacity planning, persistence, lifetime and serialized
  access. Do not call the underlying store directly when integrity is required.
- IEEE CRC-32 detects accidental corruption and all single-bit payload errors.
  It is not a signature, MAC or defense against an adversary able to change
  both data and checksum. It does not replace ECC, authenticated storage or a
  trusted manifest.
- This host-side storage primitive does not establish live RAM or VRAM savings,
  GPU residency, synchronization, device safety or physical Switch acceptance.

The standalone regression covers zero, sparse, word-run and raw pages; every
retained payload byte is flipped independently and must be rejected before the
destination changes. It also covers checksum/offset corruption, exact capacity,
guard bytes and failure atomicity.
