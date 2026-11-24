# Import a local Steam library

From the EXODUSx1 checkout on a PC, run the importer with Python 3.8 or newer. Pass each library that you want to scan. A library root and its `steamapps` directory are both accepted.

```shell
python exodus/steam_catalog.py "D:/SteamLibrary" "E:/Games/Steam/steamapps" > catalog.json
```

The JSON document has a `schema_version` and an `entries` array. Each entry contains the provider `steam`, the decimal App ID as a string, and the display name from the manifest. When supplied by the manifest, `last_played` contains Unix time in seconds. Missing or zero timestamps and unavailable artwork are exported as `null`. The pair of provider and ID identifies a catalog entry. App IDs are sorted numerically.

`installation_state` describes what the PC manifest reported. A set FullyInstalled flag yields `reported_installed`, a clear flag yields `not_fully_installed`, and a missing flag yields `unknown`. If multiple libraries contain the same app, the importer prefers those states in that order. The first library wins ties. A copied manifest can outlive its game files. Switch compatibility stays `unknown` and `launch_mode` stays `null` until a runtime provides evidence.

The command reads local manifests without signing in or contacting Steam. Only catalog fields are exported. The output omits account data, library paths and launch commands. Exit code 0 indicates a complete scan, including an empty library. If an input is unreadable or a manifest is malformed, exceeds 16 MiB or is not a regular file, the command writes diagnostics to stderr, emits the entries it could read and exits with code 1.
