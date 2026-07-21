#!/usr/bin/env node
// Clawdmeter WiFi usage service.
//
// Serves the SAME short-key JSON the BLE daemon used to write over GATT, but
// over HTTP at GET /usage, so the ESP32 firmware's parse_json() is untouched.
// The OAuth token never leaves this machine.
//
// Data source: GET https://api.anthropic.com/api/oauth/usage — one authenticated
// GET that returns session (5h), weekly (7d), and per-model weekly limits
// (weekly_scoped, e.g. Fable) in clean JSON. Consumes no inference quota.
//
// Responsibilities:
//   1. Read ~/.claude/.credentials.json (claudeAiOauth), refreshing the access
//      token via the OAuth refresh_token grant when near expiry and writing the
//      rotated tokens back atomically (stays in sync with the CLI, same file).
//   2. Poll the usage endpoint every POLL_INTERVAL and build the payload.
//   3. Cache it and serve at http://<host>:PORT/usage.
//
// Node 18+ (uses global fetch). No external dependencies.

import { readFile, writeFile, rename } from 'node:fs/promises';
import { createServer } from 'node:http';
import { homedir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// ---- config -----------------------------------------------------------------
const PORT = Number(process.env.CLAWD_PORT || 8090);
const POLL_INTERVAL = Number(process.env.CLAWD_POLL_INTERVAL || 60) * 1000;
const CREDS_FILE =
  process.env.CLAWD_CREDS || join(homedir(), '.claude', '.credentials.json');
const CONFIG_FILE =
  process.env.CLAWD_CONFIG ||
  join(homedir(), '.config', 'claude-usage-monitor', 'config');

const OAUTH_TOKEN_URL = 'https://console.anthropic.com/v1/oauth/token';
const OAUTH_CLIENT_ID =
  process.env.ANTHROPIC_CLIENT_ID || '9d1c250a-e61b-44d9-88ed-5944d1962f5e';
const USAGE_URL = 'https://api.anthropic.com/api/oauth/usage';

// ---- tiny logger (never logs token material) --------------------------------
const log = (msg) => {
  const t = new Date().toISOString().slice(11, 19);
  console.log(`[${t}] ${msg}`);
};

// ---- config file (chime / clock), re-read every poll like the bash daemon ---
async function readConfig() {
  const cfg = { chime: 'off', clock: 'off' };
  let raw = '';
  try {
    raw = await readFile(CONFIG_FILE, 'utf8');
  } catch {
    return cfg;
  }
  for (const line of raw.split('\n')) {
    const m = line.match(/^\s*(chime|clock)\s*=\s*([^#\r\n]*)/);
    if (m) cfg[m[1]] = m[2].trim().toLowerCase();
  }
  if (!['on', 'off'].includes(cfg.chime)) cfg.chime = 'off';
  if (!['off', 'auto', '12', '24'].includes(cfg.clock)) cfg.clock = 'off';
  return cfg;
}

// ---- credentials + OAuth refresh --------------------------------------------
async function loadCreds() {
  const raw = await readFile(CREDS_FILE, 'utf8');
  const doc = JSON.parse(raw);
  if (!doc.claudeAiOauth?.accessToken) {
    throw new Error('no claudeAiOauth.accessToken in credentials file');
  }
  return doc;
}

function isExpired(oauth) {
  if (!oauth?.expiresAt) return true;
  return Date.now() + 60_000 >= oauth.expiresAt; // 60s skew
}

// Refresh rotates the refresh_token, so we must persist it. Re-read the file
// first (the CLI on this box may have refreshed already), write atomically.
let refreshInFlight = null;
async function ensureFreshToken() {
  const doc = await loadCreds();
  if (!isExpired(doc.claudeAiOauth)) return doc.claudeAiOauth.accessToken;

  if (!refreshInFlight) {
    refreshInFlight = (async () => {
      const oauth = doc.claudeAiOauth;
      if (!oauth.refreshToken) throw new Error('token expired and no refreshToken');
      log('access token near/at expiry — refreshing');
      const res = await fetch(OAUTH_TOKEN_URL, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({
          grant_type: 'refresh_token',
          refresh_token: oauth.refreshToken,
          client_id: OAUTH_CLIENT_ID,
        }),
      });
      if (!res.ok) {
        throw new Error(`refresh failed: HTTP ${res.status} ${await res.text()}`);
      }
      const j = await res.json();
      // Re-read to preserve any concurrent CLI edits, then merge the new tokens
      // and every non-token field (scopes, subscriptionType, rateLimitTier…).
      let latest;
      try {
        latest = await loadCreds();
      } catch {
        latest = doc;
      }
      latest.claudeAiOauth = {
        ...latest.claudeAiOauth,
        accessToken: j.access_token,
        refreshToken: j.refresh_token || oauth.refreshToken,
        expiresAt: Date.now() + Number(j.expires_in ?? 3600) * 1000,
        ...(typeof j.scope === 'string'
          ? { scopes: j.scope.split(' ').filter(Boolean) }
          : {}),
      };
      const tmp = `${CREDS_FILE}.tmp-${process.pid}`;
      await writeFile(tmp, JSON.stringify(latest, null, 2), { mode: 0o600 });
      await rename(tmp, CREDS_FILE); // atomic on POSIX
      log('token refreshed and written back');
      return latest.claudeAiOauth.accessToken;
    })().finally(() => (refreshInFlight = null));
  }
  return refreshInFlight;
}

// ---- clock fragment (mirrors the bash daemon) -------------------------------
function clockFields(clock) {
  if (clock === 'off') return {};
  const now = Date.now();
  const offsetMin = -new Date().getTimezoneOffset(); // local offset in minutes
  const localEpoch = Math.floor(now / 1000) + offsetMin * 60;
  let tf = 24;
  if (clock === '12') tf = 12;
  else if (clock === '24') tf = 24;
  else {
    try {
      const opts = new Intl.DateTimeFormat(undefined, { hour: 'numeric' }).resolvedOptions();
      tf = opts.hour12 ? 12 : 24;
    } catch {
      tf = 24;
    }
  }
  return { t: localEpoch, tf };
}

// ---- usage endpoint -> firmware payload -------------------------------------
const round = (v) => Math.round(Number(v) || 0);

const resetMinsIso = (iso) => {
  if (!iso) return 0;
  const t = Date.parse(iso);
  if (Number.isNaN(t)) return 0;
  const m = Math.round((t - Date.now()) / 60000);
  return m > 0 ? m : 0;
};

// Anthropic severity -> the status string the firmware displays.
const sevToStatus = (sev) => {
  switch (sev) {
    case 'warning': return 'allowed_warning';
    case 'critical': return 'limited';
    case 'normal': return 'allowed';
    default: return sev || 'allowed';
  }
};

async function buildPayload() {
  const token = await ensureFreshToken();
  const cfg = await readConfig();

  const res = await fetch(USAGE_URL, {
    headers: {
      Authorization: `Bearer ${token}`,
      'anthropic-beta': 'oauth-2025-04-20',
      'anthropic-version': '2023-06-01',
      'user-agent': 'claude-code/2.1.5',
    },
  });
  if (!res.ok) throw new Error(`usage endpoint HTTP ${res.status} ${await res.text()}`);
  const u = await res.json();
  if (process.env.CLAWD_DUMP_HEADERS) log(`usage raw: ${JSON.stringify(u)}`);

  const limits = Array.isArray(u.limits) ? u.limits : [];
  const byKind = (k) => limits.find((l) => l?.kind === k);
  const session = byKind('session');
  const weeklyAll = byKind('weekly_all');
  const fiveH = u.five_hour || {};
  const sevenD = u.seven_day || {};

  const extra = { ...clockFields(cfg.clock), ...(cfg.chime === 'on' ? { c: 1 } : {}) };

  // Per-model weekly bar (weekly_scoped). Today that's Fable; the entry is
  // simply absent when the model isn't in the subscription, so the firmware
  // hides the bar. We surface the first one as f/fr/fn.
  const scoped = limits.find(
    (l) => l?.kind === 'weekly_scoped' && typeof l.percent === 'number'
  );
  const fable = scoped
    ? {
        f: round(scoped.percent),
        fr: resetMinsIso(scoped.resets_at),
        fn: scoped.scope?.model?.display_name || 'Model',
      }
    : {};

  return {
    s: round(fiveH.utilization ?? session?.percent ?? 0),
    sr: resetMinsIso(fiveH.resets_at ?? session?.resets_at),
    w: round(sevenD.utilization ?? weeklyAll?.percent ?? 0),
    wr: resetMinsIso(sevenD.resets_at ?? weeklyAll?.resets_at),
    st: sevToStatus(session?.severity),
    acct: 'pro', // subscription (5h/7d windows). Dollar-limit orgs not handled here.
    ...fable,
    ...extra,
    ok: true,
  };
}

// ---- static assets for the web view (PWA) -----------------------------------
// A phone/watch-friendly view of the same data at GET /. Purely additive: the
// firmware's /usage contract is untouched.
//
// Routes are an explicit allowlist mapping URL -> file on disk, rather than a
// directory server, so no request can ever escape into the repo (or ~/.claude,
// which sits next door and holds the OAuth tokens).
const HERE = dirname(fileURLToPath(import.meta.url));
const PUBLIC = join(HERE, 'public');
const ASSETS = join(HERE, '..', 'assets');

const HTML = 'text/html; charset=utf-8';
const IMMUTABLE = 'public, max-age=31536000, immutable';
const NO_CACHE = 'no-cache';   // revalidate: lets an edited page/SW reach the phone

// null-prototype: a plain object literal would make GET /constructor (and
// friends) resolve to an inherited Object.prototype member. Today's keys all
// start with "/" so that can't collide, but that's an easy invariant to break.
const STATIC = Object.assign(Object.create(null), {
  '/':                      [join(PUBLIC, 'view.html'), HTML, NO_CACHE],
  '/index.html':            [join(PUBLIC, 'view.html'), HTML, NO_CACHE],
  // Must revalidate, or a stale worker pins the app to an old shell forever.
  '/sw.js':                 [join(PUBLIC, 'sw.js'), 'text/javascript; charset=utf-8', NO_CACHE],
  '/manifest.webmanifest':  [join(PUBLIC, 'manifest.webmanifest'), 'application/manifest+json', NO_CACHE],
  '/icon-192.png':          [join(PUBLIC, 'icon-192.png'), 'image/png', IMMUTABLE],
  '/icon-512.png':          [join(PUBLIC, 'icon-512.png'), 'image/png', IMMUTABLE],
  // Served straight from assets/ — the same art and typefaces the panel uses.
  '/logo.png':              [join(ASSETS, 'logo_80.png'), 'image/png', IMMUTABLE],
  '/font/styrene.otf':      [join(ASSETS, 'StyreneB-Regular.otf'), 'font/otf', IMMUTABLE],
  '/font/tiempos.otf':      [join(ASSETS, 'TiemposText-400-Regular.otf'), 'font/otf', IMMUTABLE],
});

async function serveStatic(entry, res) {
  const [file, type, cache] = entry;
  try {
    const body = await readFile(file);   // small files, read per request — the SW caches client-side
    res.writeHead(200, { 'content-type': type, 'cache-control': cache });
    res.end(body);
  } catch (e) {
    log(`static ${file}: ${e.code || e.message}`);
    res.writeHead(404, { 'content-type': 'text/plain' });
    res.end('not found\n');
  }
}

// ---- refresh loop + HTTP server ---------------------------------------------
let cached = null;
let lastError = null;
let lastOkAt = 0;

async function refresh() {
  try {
    cached = await buildPayload();
    lastError = null;
    lastOkAt = Date.now();
    log(`usage: ${JSON.stringify(cached)}`);
  } catch (e) {
    lastError = String(e.message || e);
    log(`poll error: ${lastError}`);
  }
}

const server = createServer((req, res) => {
  const url = (req.url || '').split('?')[0];
  if (url === '/usage') {
    if (cached) {
      res.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      res.end(JSON.stringify(cached));
    } else {
      res.writeHead(503, { 'content-type': 'application/json' });
      res.end(JSON.stringify({ ok: false, error: lastError || 'no data yet' }));
    }
    return;
  }
  if (url === '/health') {
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ ok: !!cached, lastOkAt, lastError }));
    return;
  }
  const asset = STATIC[url];
  if (asset) {
    serveStatic(asset, res);
    return;
  }
  res.writeHead(404, { 'content-type': 'text/plain' });
  res.end('not found\n');
});

server.listen(PORT, '0.0.0.0', () => {
  log(`Clawdmeter usage service on http://0.0.0.0:${PORT}/usage`);
  refresh();
  setInterval(refresh, POLL_INTERVAL);
});
