# EXODUS standalone CorkDeck UI R02

This is an experimental host build target for #18/#40, not a HOME replacement or a Switch binary. It links the original CorkDeck drawing, audio, SVG and input primitives, without linking launcher.c, shell.inc, Steam, FEX, Box64 or Wine. The settings preview uses the donor's real grouped settings widget rather than reproducing it.

## Reproduce

Requires a C compiler, git, pkg-config, SDL2 (2.0.18 or later), SDL2_ttf, and a user-provided TTF font. From EXODUS root:

```sh
git clone https://github.com/Garyu-tt/CorkDeck.git
git -C CorkDeck checkout --detach adc0b52284e9e57d18808427a770a2a162c76bf0
sh exodus/home/corkdeck/build-host.sh CorkDeck /tmp/exodus-settings
/tmp/exodus-settings /path/to/font.ttf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/exodus-settings /path/to/font.ttf --smoke
```

The smoke mode draws one frame and exits; it is not an input or hardware test. The normal mode uses donor focus navigation and grouped rows. Platform and battery are reported by SDL when available, not by a simulated Switch. Other settings are explicitly unavailable pending Horizon adapters; there are no writes.

The donor is pinned and referenced, not copied into EXODUS. Preserve CorkDeck/Wine LGPL notices and third-party notices; review per-file provenance (including the SVG and focus-border implementation) before shipping linked binaries. No distribution of donor assets or binaries is part of this change.

## Gates

- Compile and run the host smoke and interactive settings screen.
- Integrate catalog provider adapter (#46) and native title inventory (#15).
- Build separately with devkitPro and Switch SDL2; validate applet/service permissions and hardware status.
- Keep Latitude/uLaunch fallback until device acceptance.
- Extract further actual CorkDeck shell screens incrementally; shell.inc currently relies on launcher state and is not standalone.

No successful build or device run is claimed by this PR.
