# Clawdmeter WiFi usage service

Serves the same short-key usage JSON the BLE daemon used to write over GATT, but
over HTTP so a WiFi-only ESP32 can poll it. **The OAuth token never leaves this
machine.**

```
ESP32 (WiFi)  ──GET /usage every 60s──►  this service  ──►  api.anthropic.com
phone / PWA   ──GET /       ─────────►
```

## What it does

1. Reads `~/.claude/.credentials.json`, and when the access token is near expiry
   uses the OAuth `refresh_token` grant to mint a new one, writing the rotated
   tokens back atomically (so it stays in sync with the Claude Code CLI, which
   shares the same file). The stock daemon never refreshed — it just 401'd.
2. Polls `api.anthropic.com/v1/messages` every 60s and scrapes the
   `anthropic-ratelimit-unified-*` response headers.
3. Caches the result and serves it at `GET /usage`.

## Run

```bash
node usage-service.mjs                 # foreground, binds 0.0.0.0:8090
curl http://localhost:8090/usage       # {"s":63,"sr":122,"w":76,"wr":242,"st":"allowed","acct":"pro","ok":true}
curl http://localhost:8090/health      # {"ok":true,"lastOkAt":...,"lastError":null}
```

### As a service (survives reboots)

```bash
mkdir -p ~/.config/systemd/user
cp clawdmeter-usage.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now clawdmeter-usage
loginctl enable-linger "$USER"         # so it runs without an active login
journalctl --user -u clawdmeter-usage -f
```

## Config (env)

| var                  | default                          | meaning                                  |
|----------------------|----------------------------------|------------------------------------------|
| `CLAWD_PORT`         | `8090`                           | HTTP listen port (8080 is used by n8n)   |
| `CLAWD_POLL_INTERVAL`| `60`                             | seconds between Anthropic polls          |
| `CLAWD_CREDS`        | `~/.claude/.credentials.json`    | OAuth credentials file                   |
| `CLAWD_CONFIG`       | `~/.config/claude-usage-monitor/config` | optional chime/clock config (see `daemon/config.example`) |
| `CLAWD_DUMP_HEADERS` | unset                            | set to `1` to log every `anthropic-ratelimit-*` header once per poll (header discovery) |

## Web view / PWA (`GET /`)

The same three bars the ESP32 renders, as a web page — for a phone home-screen
icon when the device isn't in front of you. Reuses the panel's own design
tokens (`firmware/src/theme.h`), Styrene/Tiempos typefaces and pixel-art logo
from `assets/`, and ports `pct_color()` / `format_reset_time()` / the animated
status line from `firmware/src/ui.cpp`, so it reads as the same product.

**Install on Android:** open `http://<host>:8090/` in Chrome → ⋮ → *Add to Home
screen*. It launches standalone (no browser chrome). Over Tailscale, use the
tailnet name/IP and it works anywhere.

Behaviour worth knowing:

- Polls `/usage` every 30s, and immediately on reopen (backgrounded tabs get
  throttled hard, so the interval alone would show a stale first frame).
- Reset countdowns tick down locally between polls instead of freezing.
- The Fable panel appears/disappears with the payload's `f` key, mirroring the
  firmware's 3-panel ↔ 2-panel reflow.
- **Stale data is never shown as live.** If the fetch fails the panels stay but
  the header switches to `Stale · Nm ago` — same contract as the firmware's
  idle "Zzz" screen. The service worker caches the shell for instant open but
  deliberately never caches `/usage`.

Files: `public/view.html` (page), `public/sw.js` (shell cache),
`public/manifest.webmanifest`, `public/icon-{192,512}.png` (regenerate with
`python3 tools/make_pwa_icons.py`). Routes are an explicit allowlist in
`usage-service.mjs` — not a directory server — so no request can walk out into
the repo or `~/.claude`.

## Payload schema

Identical to the BLE daemon's GATT payload — see `firmware/src/data.h` /
`parse_json()` in `firmware/src/main.cpp`. Keys: `s`, `sr`, `w`, `wr`, `st`,
`acct` (`pro`|`ent`), optional `c` (chime), `t`/`tf` (clock), Enterprise
`tp`/`pd`/`rd`, and `ok`.
