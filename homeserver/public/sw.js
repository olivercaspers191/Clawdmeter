// Service worker for the Clawdmeter web view.
//
// Its only job is to make the app *open* instantly and work offline-ish: the
// shell (HTML, fonts, icons) is cache-first, so tapping the home-screen icon
// paints immediately even off the tailnet.
//
// /usage is deliberately NEVER cached. The firmware refuses to render stale
// numbers as if they were live (it drops to the idle "Zzz" screen instead), and
// this view keeps that contract — a failed fetch surfaces as a "Stale · Nm ago"
// header, not as a confidently wrong percentage from an hour ago.

const CACHE = 'clawdmeter-v1';
const SHELL = [
  '.',
  'manifest.webmanifest',
  'logo.png',
  'icon-192.png',
  'icon-512.png',
  'font/styrene.otf',
  'font/tiempos.otf',
];

self.addEventListener('install', (e) => {
  // addAll fails the whole install if any entry 404s; tolerate that so a
  // missing optional asset can't leave the app permanently un-installable.
  e.waitUntil(
    caches.open(CACHE)
      .then((c) => Promise.allSettled(SHELL.map((u) => c.add(u))))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== 'GET' || url.origin !== self.location.origin) return;

  // Live data: network only, never served from cache. See the note above.
  if (url.pathname.endsWith('/usage') || url.pathname.endsWith('/health')) return;

  // Shell: cache-first, refreshing the entry in the background so an edited
  // page reaches the device on the next open rather than needing a reinstall.
  e.respondWith(
    caches.match(e.request).then((hit) => {
      const net = fetch(e.request)
        .then((res) => {
          if (res && res.ok) {
            const copy = res.clone();
            caches.open(CACHE).then((c) => c.put(e.request, copy));
          }
          return res;
        })
        .catch(() => hit);
      return hit || net;
    })
  );
});
