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
        """    /* Two independent requests: sequence, value, generation. */
    mb[9] = 42u;
    mb[10] = 0xFFFFFFFFu;
    mb[11] = 7u;
    mb[16] = 43u;
    mb[17] = 0xFFFFFFFFu;
    mb[18] = 6u;
    mb[19] = 0xA5A5A5A5u;
    mb[20] = 0x5A5A5A5Au;
    mb[21] = 0u;
    mb[22] = 0u;
    /* Full four-slot synthetic ring, starting at its last slot. */
    mb[24] = 3u;  /* consumer */
    mb[25] = 7u;  /* producer */
    mb[26] = 0u;  /* completed */
    mb[27] = 100u; /* last accepted sequence */
    /* Slots in consumption order 3, 0, 1, 2. */
    mb[44] = 101u; mb[45] = 7u; mb[46] = 10u; mb[47] = 99u;
    mb[32] = 101u; mb[33] = 7u; mb[34] = 20u; mb[35] = 99u;
    mb[36] = 102u; mb[37] = 6u; mb[38] = 30u; mb[39] = 99u;
    mb[40] = 102u; mb[41] = 7u; mb[42] = 40u; mb[43] = 99u;
    /* The bring-up, in ccplex_boot_cpu0() order (Erista). */""")
    c = replace_once(c, '    hold_cpu0();',
        """    /* Completion marker is written by CPU0 after these reply fields. */
    if (mb[0] == KCPU_MAGIC && mb[12] == 0u && mb[13] == 1u &&
        mb[14] == 42u && mb[15] == 0u &&
        mb[19] == 0xA5A5A5A5u && mb[20] == 0x5A5A5A5Au &&
        mb[21] == 43u && mb[22] == 1u)
        puts_("EXODUS IPC REFERENCE: PASS\\n");
    else
        puts_("EXODUS IPC REFERENCE: FAIL\\n");
    if (mb[0] == KCPU_MAGIC && mb[24] == 7u && mb[25] == 7u &&
        mb[26] == 4u && mb[27] == 102u &&
        mb[46] == 11u && mb[47] == 0u &&
        mb[34] == 20u && mb[35] == 2u &&
        mb[38] == 30u && mb[39] == 1u &&
        mb[42] == 41u && mb[43] == 0u)
        puts_("EXODUS RING REFERENCE: PASS\\n");
    else
        puts_("EXODUS RING REFERENCE: FAIL\\n");
    hold_cpu0();""")
    asm = replace_once(peer.read_text(), "        ldr     w2, =0x4B435055",
        """        /* EXODUS synthetic request/reply; no native pointers. */
        ldr     w13, [x1, #36]         /* sequence */
        ldr     w14, [x1, #40]         /* zero extends into x14 */
        ldr     w15, [x1, #44]         /* generation */
        cmp     w15, #7
        b.ne    exodus_first_reject
        add     x14, x14, #1
        str     w14, [x1, #48]         /* low result word */
        lsr     x14, x14, #32
        str     w14, [x1, #52]         /* high result word */
        str     w13, [x1, #56]         /* echoed sequence */
        str     wzr, [x1, #60]         /* accepted */
        b       exodus_second
exodus_first_reject:
        mov     w15, #1
        str     w15, [x1, #60]
exodus_second:
        ldr     w13, [x1, #64]
        ldr     w15, [x1, #72]
        str     w13, [x1, #84]
        cmp     w15, #7
        b.ne    exodus_second_reject
        ldr     w14, [x1, #68]
        add     x14, x14, #1
        str     w14, [x1, #76]
        lsr     x14, x14, #32
        str     w14, [x1, #80]
        str     wzr, [x1, #88]
        b       exodus_complete
exodus_second_reject:
        mov     w15, #1
        str     w15, [x1, #88]         /* no result writes */
exodus_complete:
        /* Bounded ring: unsigned occupancy, no writes on invalid geometry. */
        ldr     w17, [x1, #96]
        ldr     w18, [x1, #100]
        ldr     w19, [x1, #108]
        sub     w20, w18, w17
        cmp     w20, #4
        b.hi    exodus_ring_done
        add     x16, x1, #128
exodus_ring_next:
        cmp     w17, w18
        b.eq    exodus_ring_done
        and     w20, w17, #3
        add     x21, x16, w20, uxtw #4
        ldr     w22, [x21]
        ldr     w23, [x21, #4]
        cmp     w23, #7
        b.ne    exodus_ring_stale
        cmp     w22, w19
        b.ls    exodus_ring_duplicate
        ldr     w24, [x21, #8]
        add     w24, w24, #1
        str     w24, [x21, #8]
        mov     w19, w22
        str     w19, [x1, #108]
        mov     w24, #0
        b       exodus_ring_reply
exodus_ring_stale:
        mov     w24, #1
        b       exodus_ring_reply
exodus_ring_duplicate:
        mov     w24, #2
exodus_ring_reply:
        str     w24, [x21, #12]
        dmb     sy
        add     w17, w17, #1
        str     w17, [x1, #96]
        ldr     w24, [x1, #104]
        add     w24, w24, #1
        str     w24, [x1, #104]
        b       exodus_ring_next
exodus_ring_done:
        dmb     sy
        ldr     w2, =0x4B435055""")
    # All replacements checked before modifying either file.
    host.write_text(c)
    peer.write_text(asm)
    print("EXODUS synthetic IPC fixture prepared")

if __name__ == "__main__":
    main()
