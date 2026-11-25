# Import a local Steam library

From the EXODUSx1 checkout on a PC, run the importer with Python 3.8 or newer. Pass each library that you want to scan. A library root and its `steamapps` directory are both accepted.

```shell
python exodus/steam_catalog.py "D:/SteamLibrary" "E:/Games/Steam/steamapps" --output catalog.json
```

Use `--output` for direct file output in Windows PowerShell 5.1. Its `>` redirection encodes the catalog as UTF-16LE, which can push a valid catalog over the HOME byte limit. Omit `--output` to send JSON to stdout.

The JSON document has a `schema_version` and an `entries` array. Each entry contains the provider `steam`, the decimal App ID as a string, and the display name from the manifest. When supplied by the manifest, `last_played` contains Unix time in seconds. Missing or zero timestamps and unavailable artwork are exported as `null`. The pair of provider and ID identifies a catalog entry. App IDs are sorted numerically.

`installation_state` describes what the PC manifest reported. A set FullyInstalled flag yields `reported_installed`, a clear flag yields `not_fully_installed`, and a missing flag yields `unknown`. If multiple libraries contain the same app, the importer prefers those states in that order. The first library wins ties. A copied manifest can outlive its game files. Switch compatibility stays `unknown` and `launch_mode` stays `null` until a runtime provides evidence.

The command reads local manifests without signing in or contacting Steam. Only catalog fields are exported. The output omits account data, library paths and launch commands. Exit code 0 indicates a complete scan, including an empty library. If an input is unreadable or a manifest is malformed, exceeds 16 MiB or is not a regular file, the command writes diagnostics to stderr, emits the entries it could read and exits with code 1.

## Discover libraries from a Steam installation

Pass `--steam-install` to read the libraries configured by an installed Steam client. Supply the installation root, such as `C:/Program Files (x86)/Steam`, rather than its `steamapps` directory. The option may be repeated and is available in both catalog commands:

```shell
python exodus/steam_catalog.py --steam-install "C:/Program Files (x86)/Steam" --output steam.json
python -m exodus.app_catalog --steam-install "C:/Program Files (x86)/Steam" --switch-sd F:/ --output catalog.json
```

Discovery reads `steamapps/libraryfolders.vdf`, falling back to `config/libraryfolders.vdf` only when the first file is absent. It accepts numbered library objects with a `path` field and older numbered path strings. The installation's default `steamapps` library is always scanned. Additional libraries follow their numeric configuration indexes; unrelated configuration metadata is ignored. Duplicate library paths, including local symlink aliases, are scanned once. Explicit positional libraries or `--steam-library` sources precede discovered libraries when equally installed copies of an app appear in more than one location.

Configured paths must be absolute paths for the host running the command. Empty, relative and foreign-platform paths are reported as errors; paths are not expanded from environment variables or translated between Windows and Linux. A missing, unreadable or invalid configuration, an invalid library path or an unavailable drive produces a partial catalog and exit code 1 while preserving readable entries from the default library and other sources. A malformed preferred configuration does not silently select an older fallback file. Configuration files have the same 16 MiB bound as manifests. An empty `LibraryFolders {}` object is valid.

Discovery is opt-in: the existing positional and `--steam-library` inputs still scan only the specified libraries. It reads manifests without account access or Steam API calls, does not launch applications and does not export configuration paths or metadata.

## Combine Steam and Switch homebrew

From the checkout root on a PC, pass the mounted Switch SD card root and any Steam libraries to scan.

```shell
python -m exodus.app_catalog --switch-sd F:/ --steam-library "D:/SteamLibrary" --output catalog.json
```

Supply either source on its own or repeat `--steam-library` for additional Steam libraries. The catalog keeps both providers in the same `entries` array. It lists Steam entries by numeric App ID, followed by `switch_homebrew` entries sorted by their paths relative to the SD card's `switch` directory. That relative path uses forward slashes and is the provider ID.

The homebrew scan reads `.nro` metadata under `switch` and uses the first nonempty UTF-8 title from the embedded NACP data. It supports both plain and DEFLATE-compressed NACP title tables. Files without a title use their filename stem. A readable NRO is marked `reported_installed` based on its presence on the SD card, with compatibility unknown and launch mode unset. The scan emits readable entries and returns 1 if it encounters unreadable inputs or malformed metadata.

## Catalog output limits

Both offline commands check the complete combined catalog against HOME's limits of 4,096 entries and 2 MiB before writing any output. The byte budget includes JSON formatting, Unicode escapes and the final newline. The commands emit ASCII JSON with LF line endings on both Windows and Linux, so Windows text-mode newline expansion cannot push an otherwise valid document over the limit.

An oversized catalog produces a diagnostic on stderr, exits with code 1 and writes no JSON to stdout. No entries are silently dropped. Select fewer source libraries or correct oversized source metadata before exporting again. Ordinary partial scans still emit their readable entries with exit code 1 when the resulting catalog fits these limits. Steam sign-in refreshes and both offline importers' `--output` options use the same checks and atomic file replacement to preserve any existing snapshot on a size or write error.

## Refresh Steam titles after browser sign-in

On a PC, supply your Steam Web API key in the command's `STEAM_WEB_API_KEY` environment variable and run the sync from the checkout root. The system browser opens Steam Community for sign-in. Steam handles credentials and Steam Guard on its own page. [Steam Web API keys](https://partner.steamgames.com/doc/webapi_auth) · [Steam OpenID](https://partner.steamgames.com/doc/features/auth)

```shell
python -m exodus.steam_sync --output steam-catalog.json
```

The command verifies the one-time browser callback with Steam and uses `GetOwnedGames` for the verified SteamID. The key and the account's game-details visibility determine which library data the API returns. An inaccessible library, a cancelled sign-in or a failed request leaves any previous output in place. An explicitly empty library is exported as an empty Steam list. [Steam GetOwnedGames](https://partner.steamgames.com/doc/webapi/IPlayerService#GetOwnedGames)

`--timeout SECONDS` bounds acceptance of the browser sign-in, including Steam's identity verification (default: 180 seconds). Verification must complete before the deadline; a late result leaves the previous catalog intact and does not start a library request. The verification request uses the remaining sign-in time, capped at ten seconds. Library retrieval after an accepted sign-in keeps its separate request timeout.

To refresh the Steam rows of an existing HOME snapshot, pass it as `--base-catalog`. The sync keeps the existing Switch and homebrew entries and replaces only the Steam entries. The output path can be the base catalog itself. Base snapshots may use UTF-8 or UTF-16LE with a BOM, as written by Windows PowerShell 5.1 redirection. A successful refresh writes UTF-8 JSON; invalid input preserves the existing output and stops before sign-in.

```shell
python -m exodus.steam_sync --base-catalog F:/ulaunch/exodus-catalog.json --output F:/ulaunch/exodus-catalog.json
```

The schema-v1 output stores game IDs and titles with installation and compatibility states set to `unknown`. It omits the SteamID, API key and authentication data. The combined snapshot fits the uMenu limits of 4,096 entries and 2 MiB. This flow runs on the desktop. On-console sign-in, downloads and launch remain separate EX-006 milestones. [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3)
