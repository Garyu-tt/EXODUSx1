# Steamworks license check for a publisher AppID

Run `exodus.steam_entitlement` on a secure publisher-controlled host. It verifies the Steam account through the existing Steam OpenID browser flow and sends one HTTPS `CheckAppOwnership/v4` request for the selected AppID. Valve restricts this API to publisher keys authorized for that title. The result describes the active license field and applies to the current request. Running an installed game still requires a supported, approved runtime and the relevant authorization under Nintendo Safe Mode. [Valve ownership API](https://partner.steamgames.com/doc/webapi/ISteamUser#CheckAppOwnership) · [Valve identity verification](https://partner.steamgames.com/doc/features/auth#openid) · [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55)

Configure `EXODUS_STEAM_PUBLISHER_APP_ID` and `STEAMWORKS_PUBLISHER_KEY` on that host. Use a publisher key scoped by Steamworks to the configured AppID. Choose the same AppID on the command line. The key goes directly to the Steamworks partner endpoint over HTTPS in the `X-WebAPI-Key` header. The process removes its Steam key variables from its environment before starting the browser. [Valve publisher key documentation](https://partner.steamgames.com/doc/webapi_overview/auth#publisher-keys)

```sh
python -m exodus.steam_entitlement --app-id <publisher-app-id>
```

For a known test account, add `--expect-steam-id <expected-steam-id64>` with its canonical positive decimal SteamID64. The command checks that value after Steam OpenID verifies the browser account. Steamworks receives the verified account only after the comparison succeeds. The expected and verified account IDs stay out of the command's results and diagnostics.

The command writes one JSON object with `appid` and `status`. It exits with status code 0 when Steam provides a definitive license result, 1 when the result is unavailable or the account differs, and 2 for invalid command-line usage.

A definitive result requires one explicit `Content-Length` matching the complete HTTP 200 body and a Boolean `ownsapp`. The check reports `ownership_unavailable` for transfer-coded, close-delimited or truncated replies.

| Status | Meaning |
| --- | --- |
| `active_license` | Steamworks reports an active license for the verified account and requested AppID. This can include temporary access and Steam sharing. |
| `no_active_license` | Steamworks reports that the verified account has no active license for that AppID. |
| `unsupported_app` | The requested AppID is outside the host's configured publisher scope. |
| `publisher_not_configured` | The host needs a valid publisher AppID and key. |
| `sign_in_unavailable` | Steam identity verification could not complete. |
| `account_mismatch` | Steam verified a different account from the one expected. The Steamworks ownership request is skipped. |
| `ownership_unavailable` | Steamworks access, transport or response validation failed. |

## Publisher acceptance

On the secure host, configure an authorized publisher key and choose test accounts with and without an active license for the same AppID. Supply the expected account for each check.

| Expected account and browser choice | JSON status | Exit code |
| --- | --- | --- |
| Licensed test account entered and verified | `active_license` | 0 |
| Unlicensed test account entered and verified | `no_active_license` | 0 |
| First test account expected and second test account verified | `account_mismatch` | 1 |

Run all three cases with the same publisher configuration and two distinct test accounts.

```sh
python -m exodus.steam_entitlement_acceptance --app-id <publisher-app-id> \
  --licensed-steam-id <licensed-steam-id64> --unlicensed-steam-id <unlicensed-steam-id64>
```

The runner prompts on stderr before each browser sign-in. Press Enter when ready and choose the prompted account in Steam. Complete Steam Guard in the browser if required. Enter `q` to stop. The runner stops at the first unexpected result or canceled prompt and includes the completed cases in its report.

Stdout contains one JSON object with the AppID, the run status and the expected and observed status for each completed case. Results and prompts omit account IDs and keys. Exit code 0 requires all three cases to match, including `account_mismatch` in the last case. Exit code 1 indicates a partial or failed run, and 2 indicates invalid usage. The report records that acceptance run under the launch policy described above.

The host tests use synthetic Steam replies and a loopback OpenID callback. The runner fixture observes all three verified sign-ins and two Steamworks ownership requests. The wrong-account case stops at identity verification.

```sh
python -B -m unittest tests.test_steam_entitlement tests.test_steam_entitlement_acceptance
```
