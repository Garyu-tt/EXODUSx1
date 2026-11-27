#!/usr/bin/env python3
"""Adapt the pinned upstream synthetic CPU0 fixture for EXODUS reference IPC.

Only run against the disposable emulator checkout. This is not an ADSP,
Horizon, or physical-console payload. Do not deploy the generated binary.
"""
import pathlib
import subprocess
import sys

PIN = "6e7274187861172d67e6ce2d123b89fec31bffc1"

def replace_once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError("fixture anchor missing or ambiguous: " + old)
    return source.replace(old, new, 1)

def main():
    root = pathlib.Path(sys.argv[1]).resolve()
    actual = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN:
        raise RuntimeError("unexpected upstream revision: " + actual)
    folder = root / "tests" / "kernelcpu"
    host = folder / "payload.c"
    peer = folder / "cpu0.S"
    c = replace_once(host.read_text(),
        "    /* The bring-up, in ccplex_boot_cpu0() order (Erista). */",
        """    /* Synthetic fixed-width request: sequence, value, stale generation. */
    mb[9] = 7u;
    mb[10] = 0xFFFFFFFFu;
    mb[11] = 6u;
    /* The bring-up, in ccplex_boot_cpu0() order (Erista). */""")
    c = replace_once(c, '    hold_cpu0();',
        """    /* Completion marker is written by CPU0 after these reply fields. */
    if (mb[0] == KCPU_MAGIC && mb[12] == 0u && mb[13] == 1u &&
        mb[14] == 7u && mb[15] == 1u)
        puts_("EXODUS IPC REFERENCE: PASS\\n");
    else
        puts_("EXODUS IPC REFERENCE: FAIL\\n");
    hold_cpu0();""")
    asm = replace_once(peer.read_text(), "        ldr     w2, =0x4B435055",
        """        /* EXODUS synthetic request/reply; no native pointers. */
        ldr     w13, [x1, #36]         /* sequence */
        ldr     w14, [x1, #40]         /* zero extends into x14 */
        ldr     w15, [x1, #44]         /* stale generation */
        add     x14, x14, #1
        str     w14, [x1, #48]         /* low result word */
        lsr     x14, x14, #32
        str     w14, [x1, #52]         /* high result word */
        str     w13, [x1, #56]         /* echoed sequence */
        cmp     w15, w13
        cset    w15, ne
        str     w15, [x1, #60]         /* generation mismatch flag */
        dmb     sy
        ldr     w2, =0x4B435055""")
    # All replacements checked before modifying either file.
    host.write_text(c)
    peer.write_text(asm)
    print("EXODUS synthetic IPC fixture prepared")

if __name__ == "__main__":
    main()
