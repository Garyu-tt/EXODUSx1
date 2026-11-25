# Steam device gateway

Run this gateway behind an HTTPS reverse proxy at the origin configured in the Switch client.
The Python process listens on `127.0.0.1`. Supply `STEAM_WEB_API_KEY` through the gateway process
environment and route the paths below through the proxy. Keep request URLs and bodies out of
the proxy's access logs for these routes.

```sh
python -m exodus.steam_device_gateway --origin https://exodus.example.org
```

The listener defaults to port `8787` and accepts `--port`. Steam handles the account sign-in in
the browser. The gateway sends the verified account identifier to Steam's Web API and returns
only the schema-v1 catalog. It keeps sessions in memory for up to five minutes and allows up to
32 active sessions. Apply request limits at the HTTPS proxy when using a reachable gateway.

| Request | Successful response |
| --- | --- |
| `POST /v1/steam/sessions` with `{"challenge":"…"}` | `201` and `{"session":"…"}` |
| `GET /v1/steam/login/{session}` in the browser | `302` to Steam's OpenID provider |
| Steam callback at `GET /v1/steam/callback/{state}` | `303` to `/v1/steam/done` after validation and sync |
| `GET /v1/steam/done` | Completion page |
| `POST /v1/steam/catalog` with `{"session":"…","verifier":"…"}` | One `200` response with the catalog |
| `POST /v1/steam/cancel` with `{"session":"…","verifier":"…"}` | `200` with no body; discards the session |

Send JSON POST requests with `Content-Type: application/json`. Generate a random 32-byte verifier
on the client. Send the lowercase hexadecimal SHA-256 digest of those bytes as the challenge.
Send the verifier as lowercase hexadecimal only in the catalog or cancellation request. A catalog request
with correct proof before sync completes receives `409`. A mismatched proof receives `403`. An absent, expired or already
redeemed session receives `404` from the catalog endpoint.

The native client attempts cancellation when the WebApplet cannot open or sign-in remains
incomplete after it closes. Cancellation requires the same proof as redemption and discards
either a pending session or a ready catalog, immediately freeing its slot. The old browser
login, callback and catalog URLs then return `404`; a late callback cannot recreate the session.
Missing, expired, cancelled or redeemed sessions also return `404` from the cancellation endpoint.
A failed cleanup request leaves the five-minute expiry in place. Older clients continue to use
expiry; older gateways may return `404` for cancellation without changing the client's sign-in result.

Run the local test against a loopback listener and synthetic Steam responses.

```sh
python -m unittest tests.test_steam_device_gateway
```
