# Steam sign-in and library sync on Switch

EXODUS Steam Sync opens Steam's OpenID sign-in in the Switch WebApplet through an operator-hosted gateway. The app redeems the verified session and publishes a schema-v1 Steam library snapshot on SD for the EXODUS HOME viewer. The operator's Steam Web API key belongs on the gateway. [Steam OpenID documentation](https://partner.steamgames.com/doc/features/auth) · [GetOwnedGames](https://partner.steamgames.com/doc/webapi/IPlayerService)

## Build

Use devkitA64, libnx and the nlohmann/json header used by the uLaunch adapter. Set `JSON_INCLUDE` to the directory that contains `json.hpp` if the header is outside devkitPro's Switch portlibs.

```sh
make JSON_INCLUDE=/path/to/json/single_include/nlohmann
make check JSON_INCLUDE=/path/to/json/single_include/nlohmann
```

The first command builds `exodus-steam-sync.nro`. The second runs host checks for the session flow, response limits and gateway configuration. The uMenu adapter host checks cover snapshot publication and display. The API transport uses the Switch SSL service with certificate chain, hostname and date verification. The HTTPS front end must support HTTP/1.0 requests and send an identity-encoded response with `Content-Length`. The client follows the fixed gateway protocol over TLS 1.2 or later. [libnx SSL declarations](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/ssl.h) · [Adapter host checks](../home/ulaunch/README.md#check-the-adapter-on-a-host)

## Run

Place the NRO in `/switch/exodus-steam-sync/` on the SD card and put the gateway origin on one line in `/switch/exodus-steam-sync/gateway.txt`, such as `https://gateway.example.net`. Use an origin whose certificate the Switch trusts.

Open **EXODUS Steam Sync** from a homebrew launch context with WebApplet access and press **A**. Complete Steam sign-in in the applet. If the applet stays open on the completion page, close it to return to the client. An incomplete, expired or failed session produces an error.

Install the matching patched uMenu to enable automatic refresh. A successful sync publishes a numbered Steam-only file in `/switch/exodus-steam-sync/snapshots/`. Reopen **EXODUS library** from the HOME Quick Menu to load the newest usable snapshot alongside the current Switch and homebrew entries. The viewer reads `/ulaunch/exodus-catalog.json` as the inventory source. [uMenu installation, fallback and recovery](../home/ulaunch/README.md)

A physical Switch check is needed for WebApplet sign-in, TLS trust, SD publication and HOME refresh on the target firmware. The client and adapter are licensed under [GPL-2.0-only](../home/ulaunch/COPYING).
