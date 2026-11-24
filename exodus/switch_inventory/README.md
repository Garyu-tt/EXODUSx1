# Export registered Switch applications

Build with devkitPro and libnx by running `make` in this directory from the devkitPro shell. Copy `exodus-switch-inventory.nro` to `/switch/exodus-switch-inventory/` on the SD card. Open **EXODUS Switch Inventory** from hbmenu and press **A** to export. Press **+** to exit.

The app queries Horizon's NS application records and localized control metadata. It writes a numbered `switch-titles-0001.json` snapshot in the same directory and shows the exact path. The export contains Switch title IDs and display names. A title ID is also used as the name when its metadata is unavailable. Exporting again uses the next available number.

Each entry uses the provider `switch` and a lowercase, 16-digit hexadecimal title ID. A registered title can remain in the system records when its data is unavailable, so the export leaves installation and compatibility states unknown. The console reports an error if the current homebrew context cannot access NS. The app reads system metadata and writes the snapshot on the SD card.

## Combine with a Steam catalog

On a PC, run these commands from the EXODUSx1 checkout. Replace the drive letters and filenames with your library and the export shown on the console.

```shell
python exodus/steam_catalog.py "D:/SteamLibrary" > steam-catalog.json
python exodus/catalog_merge.py "F:/switch/exodus-switch-inventory/switch-titles-0001.json" steam-catalog.json > unified-catalog.json
```

The merger accepts one or more UTF-8 schema-v1 catalogs. It preserves input order and keeps a single copy of identical entries with the same provider and ID. If catalogs disagree about an entry with the same provider and ID, the command reports the conflict and exits with code 1 before writing output. Each input is limited to 16 MiB.

The resulting JSON is an offline input for library tooling. Actual HOME integration, Steam account and downloads, and launch behavior are tracked in [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3).
