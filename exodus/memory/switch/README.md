# EXODUS Page Probe for Nintendo Switch

Build with devkitPro's Switch toolchain and libnx. Run `make` from this directory in the devkitPro shell. Copy `exodus-page-probe.nro` to `/switch/exodus-page-probe/` on the SD card, then launch **EXODUS Page Probe** from hbmenu.

The probe starts by packing three generated 4096-byte pages into one in-memory group, restoring each by index and comparing the recovered bytes. The expected payload sizes are 8 bytes for an all-zero page, 72 bytes for one occupied block and 4096 bytes for a dense page. The group shows 4176 bytes of payload and 24 bytes of offsets. Press **A** to repeat.

To measure a file, place your sample at `/switch/exodus-page-probe/sample.bin` on the SD card. Press **X** to read it in batches of up to 128 pages. The screen shows the verified full pages, sparse pages, word-run pages, input size, encoded payload and the space needed for a two-byte length index per full page. A trailing fragment counts at its original size. Press **B** to cancel a scan or **+** to return to hbmenu. The probe opens the sample read-only and uses its own buffers. Results describe the selected data. Runtime RAM/VRAM residency requires separate measurement.

The demo checks the adaptive format on a solid RGBA page. It verifies both the expected 41-byte payload and the reconstructed 4096 bytes.
