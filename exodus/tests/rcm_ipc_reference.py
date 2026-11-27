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
    mb[28] = 99u;
    /* Counter rollover: slots 2,3,0,1; four valid requests. */
    mb[48] = 0xFFFFFFFEu; mb[49] = 2u; mb[50] = 0u;
    mb[51] = 200u; mb[52] = 99u;
    for (u32 i = 0; i < 4u; i++) {
        u32 slot = 56u + ((i + 2u) & 3u) * 4u;
        mb[slot] = 201u + i; mb[slot+1] = 7u;
        mb[slot+2] = 10u + i; mb[slot+3] = 99u;
    }
    /* Empty and malformed rings; all slot words must be preserved. */
    mb[72] = 5u; mb[73] = 5u; mb[74] = 0u;
    mb[75] = 300u; mb[76] = 99u;
    mb[96] = 5u; mb[97] = 10u; mb[98] = 0u;
    mb[99] = 400u; mb[100] = 99u;
    for (u32 i = 0; i < 16u; i++) {
        mb[80u+i] = 0xA5000000u + i;
        mb[104u+i] = 0x5A000000u + i;
    }
    /* Separate mailbox for live, post-start generation handoff. */
    for (u32 i = 120u; i < 144u; i++) mb[i] = 0u;
    /* The bring-up, in ccplex_boot_cpu0() order (Erista). */""")
    c = replace_once(c, """    while (mb[0] != KCPU_MAGIC && REG(TIMERUS) - start < 200000)
        udelay(100);""",
        """    u32 live_phase = 0u;
    while (mb[0] != KCPU_MAGIC && REG(TIMERUS) - start < 200000) {
        if (live_phase == 0u && mb[120] == 1u) {
            mb[121] = 8u; mb[122] = 501u; mb[123] = 9u;
            mb[124] = 0xCCCCCCCCu; mb[125] = 99u; mb[126] = 0u;
            mb[120] = 2u;
            live_phase = 1u;
        } else if (live_phase == 1u && mb[120] == 3u) {
            if (mb[124] != 10u || mb[125] != 0u || mb[126] != 501u)
                break;
            mb[121] = 7u; mb[122] = 502u; mb[123] = 20u;
            mb[124] = 0xDDDDDDDDu; mb[125] = 99u; mb[126] = 0u;
            mb[120] = 4u;
            live_phase = 2u;
        }
        udelay(100);
    }""")
    c = replace_once(c, '    hold_cpu0();',
        """    /* Completion marker is written by CPU0 after these reply fields. */
    if (mb[0] == KCPU_MAGIC && mb[12] == 0u && mb[13] == 1u &&
        mb[14] == 42u && mb[15] == 0u &&
        mb[19] == 0xA5A5A5A5u && mb[20] == 0x5A5A5A5Au &&
        mb[21] == 43u && mb[22] == 1u)
        puts_("EXODUS IPC REFERENCE: PASS\\n");
    else
        puts_("EXODUS IPC REFERENCE: FAIL\\n");
    u32 edge_ok = mb[48] == 2u && mb[49] == 2u &&
        mb[50] == 4u && mb[51] == 204u && mb[52] == 0u &&
        mb[72] == 5u && mb[73] == 5u && mb[74] == 0u &&
        mb[75] == 300u && mb[76] == 0u &&
        mb[96] == 5u && mb[97] == 10u && mb[98] == 0u &&
        mb[99] == 400u && mb[100] == 3u;
    for (u32 i = 0; i < 4u; i++) {
        u32 slot = 56u + ((i + 2u) & 3u) * 4u;
        if (mb[slot] != 201u+i || mb[slot+1] != 7u ||
            mb[slot+2] != 11u+i || mb[slot+3] != 0u)
            edge_ok = 0u;
    }
    for (u32 i = 0; i < 16u; i++) {
        if (mb[80u+i] != 0xA5000000u+i ||
            mb[104u+i] != 0x5A000000u+i)
            edge_ok = 0u;
    }
    if (mb[0] == KCPU_MAGIC && edge_ok)
        puts_("EXODUS EDGE REFERENCE: PASS\\n");
    else
        puts_("EXODUS EDGE REFERENCE: FAIL\\n");
    u32 live_ok = live_phase == 2u && mb[120] == 5u &&
        mb[124] == 0xDDDDDDDDu && mb[125] == 1u &&
        mb[126] == 502u && mb[127] == 8u;
    if (mb[0] == KCPU_MAGIC && live_ok)
        puts_("EXODUS LIVE REFERENCE: PASS\\n");
    else
        puts_("EXODUS LIVE REFERENCE: FAIL\\n");
    if (mb[0] == KCPU_MAGIC && edge_ok && live_ok && mb[28] == 0u &&
        mb[24] == 7u && mb[25] == 7u &&
        mb[26] == 4u && mb[27] == 102u &&
        mb[46] == 11u && mb[47] == 0u &&
        mb[34] == 20u && mb[35] == 2u &&
        mb[38] == 30u && mb[39] == 1u &&
        mb[42] == 41u && mb[43] == 0u)
        puts_("EXODUS RING REFERENCE: PASS\\n");
    else
        puts_("EXODUS RING REFERENCE: FAIL\\n");
    hold_cpu0();
    /* Retained mailbox and a new CPU0 engine after an emulated reset. */
    mb[128] = 0x52535432u;
    mb[129] = 0u;
    mb[130] = 8u; mb[131] = 0xABCD1234u;
    mb[132] = 0u; mb[133] = 0u;
    mb[134] = 9u; mb[135] = 80u; mb[136] = 0u;
    mb[137] = 0xC001CAFEu;
    udelay(10000);
    u32 reset_ok = mb[129] == 0u && mb[137] == 0xC001CAFEu;
    release_cpu0();
    start = REG(TIMERUS);
    while (mb[129] != 0x52454356u && REG(TIMERUS) - start < 200000)
        udelay(100);
    reset_ok = reset_ok && mb[129] == 0x52454356u &&
        mb[131] == 0xABCD1234u && mb[132] == 1u &&
        mb[133] == 9u && mb[136] == 81u &&
        mb[137] == 0xC001CAFEu;
    hold_cpu0();
    if (reset_ok)
        puts_("EXODUS RESET REFERENCE: PASS\\n");
    else {
        puts_("EXODUS RESET REFERENCE: FAIL\\n");
        puts_("EXODUS RING REFERENCE: FAIL\\n");
    }""")
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
        /* Same consumer processes all four independent test rings. */
        add     x0, x1, #96
        bl      exodus_ring_consume
        add     x0, x1, #192
        bl      exodus_ring_consume
        add     x0, x1, #288
        bl      exodus_ring_consume
        add     x0, x1, #384
        bl      exodus_ring_consume
        b       exodus_rings_complete
exodus_ring_consume:
        /* Header: consumer, producer, completed, last ID, status.
           Slot table starts 32 bytes from the header. */
        ldr     w17, [x0]
        ldr     w18, [x0, #4]
        ldr     w19, [x0, #12]
        sub     w20, w18, w17
        cmp     w20, #4
        b.hi    exodus_ring_invalid
        str     wzr, [x0, #16]
        add     x16, x0, #32
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
        str     w19, [x0, #12]
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
        str     w17, [x0]
        ldr     w24, [x0, #8]
        add     w24, w24, #1
        str     w24, [x0, #8]
        b       exodus_ring_next
exodus_ring_invalid:
        mov     w24, #3
        str     w24, [x0, #16]
exodus_ring_done:
        ret
exodus_rings_complete:
        /* Simulated session generation changes from 7 to 8, no CPU reset. */
        add     x0, x1, #480
        mov     w17, #8
        str     w17, [x0, #28]
        dmb     sy
        mov     w18, #1
        str     w18, [x0]
exodus_live_wait_first:
        ldr     w18, [x0]
        cmp     w18, #2
        b.eq    exodus_live_first
        wfe
        b       exodus_live_wait_first
exodus_live_first:
        dmb     sy
        bl      exodus_live_reply
        dmb     sy
        mov     w18, #3
        str     w18, [x0]
exodus_live_wait_second:
        ldr     w18, [x0]
        cmp     w18, #4
        b.eq    exodus_live_second
        wfe
        b       exodus_live_wait_second
exodus_live_second:
        dmb     sy
        bl      exodus_live_reply
        dmb     sy
        mov     w18, #5
        str     w18, [x0]
        b       exodus_live_done
exodus_live_reply:
        ldr     w19, [x0, #4]
        ldr     w20, [x0, #8]
        str     w20, [x0, #24]
        cmp     w19, w17
        b.ne    exodus_live_reject
        ldr     w21, [x0, #12]
        add     w21, w21, #1
        str     w21, [x0, #16]
        str     wzr, [x0, #20]
        ret
exodus_live_reject:
        mov     w21, #1
        str     w21, [x0, #20]
        ret
exodus_live_done:
        dmb     sy
        ldr     w2, =0x4B435055""")
    asm = replace_once(asm, "_start:\\n", """_start:
        ldr     x0, =MBOX
        ldr     w1, [x0, #512]
        ldr     w2, =0x52535432
        cmp     w1, w2
        b.eq    exodus_reset_entry
""")
    asm += """
        .text
        .balign 4
exodus_reset_entry:
        /* Fresh CPU engine, retained memory; only a synthetic mailbox. */
        mov     w3, #9
        str     w3, [x0, #532]
        ldr     w4, [x0, #520]
        cmp     w4, w3
        cset    w4, ne
        str     w4, [x0, #528]
        ldr     w4, [x0, #536]
        cmp     w4, w3
        b.ne    exodus_reset_park
        ldr     w4, [x0, #540]
        add     w4, w4, #1
        str     w4, [x0, #544]
        dmb     sy
        ldr     w4, =0x52454356
        str     w4, [x0, #516]
exodus_reset_park:
        wfe
        b       exodus_reset_park
        .ltorg
"""
    # All replacements checked before modifying either file.
    host.write_text(c)
    peer.write_text(asm)
    print("EXODUS synthetic IPC fixture prepared")

if __name__ == "__main__":
    main()
