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

Send JSON POST requests with `Content-Type: application/json`. Generate a random 32-byte verifier
on the client. Send the lowercase hexadecimal SHA-256 digest of those bytes as the challenge.
Send the verifier as lowercase hexadecimal only in the catalog request. A correct proof before
sync completes receives `409`. A mismatched proof receives `403`. An absent, expired or already
redeemed session receives `404` from the catalog endpoint.

Run the local test against a loopback listener and synthetic Steam responses.

```sh
python -m unittest tests.test_steam_device_gateway
```
