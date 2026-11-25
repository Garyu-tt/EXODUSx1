# Steam file verification on Switch

EXODUS Steam File Check reads an installed game's listed files from the SD card and compares their lengths and SHA-256 hashes with a reference. It runs as a foreground homebrew application with file and byte counts. Press **B** to cancel a check. The Nintendo Safe Mode launch block remains in effect. Steam client verification covers Valve's own depot and local configuration rules. [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3) · [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55) · [Steam Support](https://help.steampowered.com/en/faqs/view/0C48-FCBD-DA71-93EB)

## Prepare the reference

On a trusted computer, use Steam's installed-files verification for the game. Close the game and wait for any Steam update to finish before generating the reference. Run this command from the EXODUSx1 checkout with the Steam library's `steamapps` path, the selected AppID and a new output path outside the game's directory.

```sh
python -m exodus.steam_integrity.reference --steamapps "D:/SteamLibrary/steamapps" --app-id 730 --output 730.files
```

Keep the reported reference SHA-256 separately from the test SD. The checksum pins the exact reference you prepared. The export records the file state at that time, including local configuration files in the chosen directory. A later update or local file edit can change the result. The exporter creates a new `.files` document and leaves the game directory intact.

Every selected source and output directory component must exist as a real directory. Parent components (`..`) are checked before normalization: an existing `directory/..` remains usable, while a missing component, regular file, symlink or Windows junction cannot be hidden by the following `..`.

The exporter rejects sibling names that differ only by per-character case folding, including ASCII and Unicode aliases such as `Σ`/`σ`. It preserves character boundaries, so distinct names such as `Straße.dat` and `STRASSE.dat` can both be exported. This remains a conservative alias check, not a model of every destination filesystem's case table. Original UTF-8 path bytes, reference ordering and file hashes are preserved.

## Run the check

1. On the test SD, place the corresponding Steam-created `appmanifest_<AppID>.acf` in `/switch/exodus-steam/steamapps/` and the installed game folder in its `common/` directory. The AppID and install folder are resolved from this manifest.
2. Put the reference in `/switch/exodus-steam/references/<AppID>.files`. Place `exodus-steam-file-check.nro` at `/switch/exodus-steam-file-check.nro` and open it from the homebrew menu.
3. Press **A** to start. Use **D-Pad Up/Down** to select the displayed AppID and **A** to confirm, **X** to enter an AppID manually, or **B** to cancel. The picker shows AppIDs in numeric order using canonical `<AppID>.files` filenames for regular files from 1 byte through 8 MiB. The app opens manual entry if the directory is unavailable, has over 512 entries, or has no candidates.
4. Enter the 64 hexadecimal digits retained separately from the SD. The filename is a selection hint. The supplied checksum pins the reference before the app reads the listed files. Compare the checked file count with the reference count after a match, or review the reported failure.

The result covers the listed files and the supplied reference. Steam entitlement and the approved local runtime remain separate requirements for launch.

Keep game updates and transfers stopped while checking. The verifier compares each opened file's identity, length and modification metadata after hashing, then reopens the current game paths and checks the same metadata before reporting a match. An observed change, replacement or removal reports the affected path when available and asks you to retry. The final metadata pass is cancellable and does not hash payloads a second time. File counts and compared bytes describe completed hash work even if a later metadata check finds a change.

This is a check of files read over time, not an atomic filesystem snapshot. It can detect only changes visible through file identity and timestamps; FAT/exFAT and other coarse-timestamp filesystems can hide same-size rewrites within their timestamp resolution. The check does not lock files or guarantee that they remain unchanged afterward.

## Build and test

Use devkitPro with devkitA64, libnx and Switch tools for the NRO. The Linux host verifier uses OpenSSL's `libcrypto`.

```sh
make -C exodus/steam_integrity
make -C exodus/steam_integrity check
python -B -m unittest tests.test_steam_integrity_reference
```

The reference is UTF-8 with LF line endings and a final LF. It starts with `EXODUS-STEAM-SHA256 1`, the decimal AppID and the file count on separate lines. Each record holds a lowercase SHA-256, decimal byte length and relative path separated by tabs. Records are sorted by path in UTF-8 byte order after folding ASCII uppercase letters. References are limited to 32,768 files and 8 MiB. The Steam app manifest has a 64 KiB limit. Each installed file's full SD path, including the library and game directories, must fit within 768 UTF-8 bytes after the `sdmc:` prefix. [libnx path handling](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/devices/fs_dev.c#L235-L247)
