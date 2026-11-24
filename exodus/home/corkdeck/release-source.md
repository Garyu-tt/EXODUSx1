# EXODUS Shell source and license

The EXODUS Shell code carries the GNU General Public License version 2 only.
The license text is in `exodus/home/ulaunch/COPYING`. The original EXODUS C
and C++ code, the independent SDL focus frame and the shared EXODUS catalog
and runtime preflight carry SPDX notices identifying their terms.

The prepared `corkdeck-donor` tree contains the launcher subset from CorkDeck
`adc0b52284e9e57d18808427a770a2a162c76bf0`, whose parent project is Autorun.
EXODUS applies GPL version 2 to this copy under section 3 of LGPL 2.1.
The selected source preserves upstream authorship, records the EXODUS changes,
and uses the copy's GPLv2 notices and license text. The included `AUTHORS`
and `LICENSE` identify the original work and the modified files. EXODUS
replaced the old focus implementation with its own stationary SDL frame.

The original UI comes from Autorun
[`5d5811d`](https://github.com/autorunhq/autorun/commit/5d5811dff49472f70d9f4ae9ac4f3da0b48099bb)
and CorkDeck
[`53e9b5f`](https://github.com/plv890/CorkDeck/commit/53e9b5fcc079a9b8e6c2f3dd681f4a05fdd9168e)
and [`2671f47`](https://github.com/plv890/CorkDeck/commit/2671f47115c0c0111e37917ef866cdb1e870aeb7).
Procedural audio and image decoding come from Autorun
[`22898bb`](https://github.com/autorunhq/autorun/commit/22898bbd014f1255dffeccb0cd2d776aae78ed40)
and [`1527b9c`](https://github.com/autorunhq/autorun/commit/1527b9c9c8a903b292b9676e924320259a539ea9).
The SVG path code comes from Autorun
[`98d4004`](https://github.com/autorunhq/autorun/commit/98d4004bd139b80e922de0af632ef4ebdf263bf4)
and implements the mathematics described in the
[SVG 1.1 implementation notes](https://www.w3.org/TR/SVG11/implnote.html#ArcConversionEndpointToCenter).

The Switch binary packages generated NACP metadata. The source selection
contains C and C++ inputs and their build scripts. The user supplies a TTF
font separately. See the included dependency materials for the source,
patches and license notices for the linked Switch libraries and runtimes.

## Rebuild

Use the Linux AMD64 variant of the devkitPro OCI image
`devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`.
Use Bookworm's `nlohmann-json3-dev=3.11.2-2` headers, Python 3, and
`pkg-config`. The pinned toolchain and portlibs are already in the image.
From the extracted source root:

```sh
sh exodus/home/corkdeck/build-switch.sh ./corkdeck-donor ./out
python3 exodus/home/corkdeck/check-release-nro.py ./out/exodus-shell.nro
sha256sum ./out/exodus-shell.nro
```

The same build command accepts a clean checkout of the pinned donor revision.
Both forms verify the prepared source files against `donor-snapshot.sha256`.
The release CI builds from the Git input and the exported source tree and
compares their ARM64 ELF and NRO outputs byte for byte.
