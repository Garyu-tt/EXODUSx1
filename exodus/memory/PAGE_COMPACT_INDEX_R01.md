# Compact index for completed page groups

Build pages in an ordinary caller-owned packed store. Once the group is
complete, call `exodus_page_compact_build` with one `uint16_t` length per page
and `exodus_page_compact_checkpoints(page_count)` slots for `size_t` checkpoints.
The resulting index references the existing encoded payload. The caller can
release the original `page_ends` array and retain the payload and compact arrays.

The index records one checkpoint per 32 pages. A random restore examines at
most 31 earlier lengths in its group, then calls the normal page decoder.
For 1,024 pages on a 64-bit target, the retained index arrays occupy 2,304
bytes instead of 8,192. That is 5,888 bytes of index savings once the
construction offsets are released. The compact index needs its own arrays
during construction.

Keep the payload and compact arrays immutable during restore. The index
validates source offsets before publishing its output and bounds each read
to the selected group. It does not add a payload checksum. The Switch probe
uses it in the generated page demo. Host verification runs via `make check`.
