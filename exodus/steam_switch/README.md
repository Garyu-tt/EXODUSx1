# Steam sign-in and library sync on Switch

EXODUS Steam Sync opens Steam's OpenID sign-in in the Switch WebApplet through an operator-hosted gateway. The app redeems the verified session for a Steam library snapshot and saves a schema-v1 EXODUS HOME catalog on SD. The operator's Steam Web API key belongs on the gateway. [Steam OpenID documentation](https://partner.steamgames.com/doc/features/auth) · [GetOwnedGames](https://partner.steamgames.com/doc/webapi/IPlayerService)

## Build

Use devkitA64, libnx and the nlohmann/json header used by the uLaunch adapter. Set `JSON_INCLUDE` to the directory that contains `json.hpp` if the header is outside devkitPro's Switch portlibs.

```sh
make JSON_INCLUDE=/path/to/json/single_include/nlohmann
make check JSON_INCLUDE=/path/to/json/single_include/nlohmann
```

The first command builds `exodus-steam-sync.nro`. The second runs host checks for the session flow, response limits and snapshot publication. The API transport uses the Switch SSL service with certificate chain, hostname and date verification. The HTTPS front end must support HTTP/1.0 requests and send an identity-encoded response with `Content-Length`. The client follows the fixed gateway protocol over TLS 1.2 or later. [libnx SSL declarations](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/ssl.h)

## Run

Place the NRO in `/switch/exodus-steam-sync/` on the SD card and put the gateway origin on one line in `/switch/exodus-steam-sync/gateway.txt`, such as `https://gateway.example.net`. Use an origin whose certificate the Switch trusts.

Open **EXODUS Steam Sync** from a homebrew launch context with WebApplet access and press **A**. Complete Steam sign-in in the applet. If the applet stays open on the completion page, close it to return to the client. An incomplete, expired or failed session produces an error.

After a successful fetch, the app reads the current `/ulaunch/exodus-catalog.json` if present. It keeps the non-Steam rows and uses the authenticated library for the new Steam rows. It publishes a new file named `exodus-catalog-*.json` in the Steam Sync directory after validation and flushing the data. To show that snapshot in EXODUS HOME, back up the current catalog and copy the selected snapshot to `/ulaunch/exodus-catalog.json`. [uMenu catalog adapter](../home/ulaunch/README.md)

A physical Switch check is needed for WebApplet sign-in, TLS trust and SD publication on the target firmware. The client and adapter are licensed under [GPL-2.0-only](../home/ulaunch/COPYING).
