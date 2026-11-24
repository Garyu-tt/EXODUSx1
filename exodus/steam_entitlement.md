# Steamworks license check for a publisher AppID

Run `exodus.steam_entitlement` on a secure publisher-controlled host. It verifies the Steam account through the existing Steam OpenID browser flow and sends one HTTPS `CheckAppOwnership/v4` request for the selected AppID. Valve restricts this API to publisher keys authorized for that title. The result describes the active license field and applies to the current request. Running an installed game still requires a supported, approved runtime and the relevant authorization under Nintendo Safe Mode. [Valve ownership API](https://partner.steamgames.com/doc/webapi/ISteamUser#CheckAppOwnership) · [Valve identity verification](https://partner.steamgames.com/doc/features/auth#openid) · [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55)

Configure `EXODUS_STEAM_PUBLISHER_APP_ID` and `STEAMWORKS_PUBLISHER_KEY` on that host. Use a publisher key scoped by Steamworks to the configured AppID. Choose the same AppID on the command line. The key goes to the Steamworks partner endpoint in the `X-WebAPI-Key` header. The process removes its Steam key variables from its environment before starting the browser. [Valve publisher key documentation](https://partner.steamgames.com/doc/webapi_overview/auth#publisher-keys)

```sh
python -m exodus.steam_entitlement --app-id <publisher-app-id>
```

The command writes one JSON object with `appid` and `status`. It exits with status code 0 when Steam provides a definitive license result, 1 when the result is unavailable, and 2 for invalid command-line usage.

| Status | Meaning |
| --- | --- |
| `active_license` | Steamworks reports an active license for the verified account and requested AppID. This can include temporary access and Steam sharing. |
| `no_active_license` | Steamworks reports that the verified account has no active license for that AppID. |
| `unsupported_app` | The requested AppID is outside the host's configured publisher scope. |
| `publisher_not_configured` | The host needs a valid publisher AppID and key. |
| `sign_in_unavailable` | Steam identity verification could not complete. |
| `ownership_unavailable` | Steamworks access, transport or response validation failed. |

The Switch device gateway and HOME catalog retain their current flows. Live acceptance uses an authorized publisher key and test accounts with and without an active license. The host tests use synthetic Steam replies and a loopback OpenID callback.

```sh
python -B -m unittest tests.test_steam_entitlement
```
