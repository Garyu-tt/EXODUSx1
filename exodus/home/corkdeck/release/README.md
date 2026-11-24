# EXODUS Shell release source

Run the package tools with Python 3.11 or later. Prepare the reviewed shell tree
using the sibling release staging script. It contains the git-sourced EXODUS
inputs, the allowlisted CorkDeck source, the SPDX notices, BUILD_SOURCE.md,
and EXODUS-SHELL-COMMIT. Build the NRO from that tree in the pinned environment
and run the shell's structural and release metadata checks.

~~~sh
release=exodus/home/corkdeck/release
python3 "$release/fetch-dependency-sources.py" \
    --cache-dir "$SOURCE_CACHE" --output-dir "$DEPENDENCIES"
python3 "$release/bundle-shell-release.py" \
    --app-source "$STAGED_SOURCE" --dependencies "$DEPENDENCIES" \
    --main-sha "$MAIN_SHA" --nro "$NRO" --output-dir "$RELEASE_OUTPUT"
~~~

The first command verifies the size and SHA-256 of every dependency input,
copies the relevant recipes and patches, and extracts the release notices.
Use --offline to reconstruct the material from a verified cache. The cache
holds the original inputs by SHA-256. The dependency output includes the pinned
manifest and its own SHA256SUMS.

The second command validates the staged commit, checks each donor file
against donor-snapshot.sha256, reruns check-release-nro.py, and verifies the
dependency directory. It writes and rereads these files before moving them into
the empty output directory.

- EXODUS-Shell-Switch-arm64.zip contains the shell NRO at its SD install path,
  the license texts and acknowledgments, and the digest of the paired source.
- EXODUS-Shell-Source.tar.gz contains the staged shell inputs, the complete
  dependency materials, the release identity, and their member checksums.
- SHA256SUMS identifies both archives.

Publish the binary and the corresponding source archive together. The packaging
checks establish source and binary identity. The separate pinned-environment
reproduction job establishes that the staged shell inputs rebuild the NRO.

## Dependency provenance

dependency-sources.json records the package versions, expected input bytes, and
upstream references for the dependency closure of the pinned native image. The
devkitPro package recipes contain the source archive checksums and Switch
patches. Mesa's package version is 20.1.0-5 and its recipe uses the upstream
20.1.0-rc3 source. The selected FreeType license option for the linked shell is
GPL version 2. Preserve the recorded notices for other components under their
respective terms.

The devkitA64 r29.2 build records specify GCC 15.2.0-7, binutils 2.45.1-2, and
newlib 4.6.0.20260123-4. GCC's archive matches the GNU release SHA-512.
Binutils' archive matches the GNU and devkitPro copies. The newlib archive and
patch match the effective checksums in the package recipe.
The Debian archive and packaging tarball match the checksums in the included
.dsc. The switch-tools source at v1.13.1 includes elf2nro and nacptool.

The original libnx archive is verified in the cache. The distributed source
copy retains the upstream file contents and omits the unused
nx/default_icon.jpg. If recreating the SDK package from this copy, create an
empty file at that path for the upstream install rule. The shell build uses
libnx's code and linker files and leaves the icon asset out of the NRO.

The pinned image supplies the build-only devkita64-rules package. Its upstream
source is available at the v1.1.1 tag of
https://github.com/devkitPro/devkita64-rules. The SHA-256 of its source
archive is bd47183a806e7721fd98cca00cb9fbea0334551882074725de484e88e4b9d13.
The paired archives contain source materials and notices for an application
built in that environment.
