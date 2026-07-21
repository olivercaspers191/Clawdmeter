# Clawdmeter Android widget

A resizable home-screen widget showing the same three usage bars as the ESP32
panel and the web view, fed by the same `GET /usage` endpoint.

```
homeserver:8090/usage ──► widget (every 15 min, + on tap)
```

## Build + install (on the homeserver)

Unlike the firmware, this builds headlessly on the Linux box — Android Studio is
just a GUI over Gradle. One command builds and serves the APK for the phone:

```bash
cd android && ./build-and-serve.sh          # release (signed); 'debug' also works
```

It prints a Tailscale URL; open it on the phone, and Chrome will ask once for
permission to install unknown apps. No cable, no Windows, nothing published
publicly. Ctrl-C when the download finishes.

### Toolchain

Installed under `~/android-build/`, entirely in `$HOME` with **no sudo** and
nothing registered system-wide — `rm -rf ~/android-build` undoes all of it:

| Piece | Path |
|---|---|
| Temurin JDK 17 | `~/android-build/jdk` |
| Android SDK (platform 35, build-tools 35.0.0, platform-tools) | `~/android-build/sdk` |
| Gradle 8.9 | `~/android-build/gradle-8.9` |

`local.properties` (git-ignored) points the build at the SDK. To rebuild the
toolchain elsewhere, point `TOOLS=` at it or set `JAVA_HOME`/`ANDROID_HOME`/`GRADLE`.

### Signing

Release builds are signed so updates install over the top of each other; an
unsigned or differently-signed APK fails with a signature mismatch and has to be
uninstalled first. **The keystore and its passwords are not in this repo** —
they're a real signing key, and anyone holding them can publish updates that
install over yours:

| What | Where |
|---|---|
| Keystore | `~/.android/clawdmeter-release.jks` |
| Passwords / alias | `~/.gradle/gradle.properties` (mode 600), as `CLAWD_*` |

Back both up. Losing them means future builds can't update an installed copy.
If those properties are absent the release build just stays unsigned, so a fresh
clone of this repo still builds.

### Android Studio

Still works if you prefer it: File → Open the `android/` folder and let it sync.
The Gradle wrapper JAR isn't committed; Studio generates it on first sync.

## Setup

Long-press the home screen → **Widgets** → *Clawdmeter*. Dropping it opens a
setup screen asking for the server address:

- `http://192.168.178.155:8090` — works at home only
- `http://<tailscale-name-or-ip>:8090` — **works anywhere**, use this one

It verifies the address before finishing, so a typo shows up there rather than
as a permanently blank widget. The address can be changed later from the
Clawdmeter icon in the app drawer.

## Sizing

`resizeMode="horizontal|vertical"` with a low `minResize`, so it takes any shape
from roughly 2x1 up to 4x4+. It isn't a set of fixed layouts — `WidgetRenderer`
computes everything from the measured size, so intermediate sizes look
deliberate rather than like the nearest tier stretched:

| Shape | What you get |
|---|---|
| Tall enough for 3 rows (~170dp+) | header (crab + "3m ago") and all three bars |
| 2x2-ish | as many bars as fit legibly, each with its "Resets in" line |
| Short (< ~68dp per panel) | bars drop the reset line and grow the number |
| Wide and short (< ~92dp tall) | panels side by side instead of stacked |

Fewer, larger panels beat cramming three into a small widget — the session bar
is the one that matters, so it's the one that survives the squeeze.

## Design notes

**Rendered to a Canvas bitmap, not a RemoteViews layout tree.** Two reasons: it
makes arbitrary resizing a matter of arithmetic rather than a pile of alternate
layouts, and it gives exact parity with the panel — real Styrene, the same
pill-radius bars, the colours straight out of `firmware/src/theme.h`. RemoteViews
can't recolour a `ProgressBar` fill per update below API 31.

**15-minute refresh is WorkManager's floor**, not a choice. It's fine for 5-hour
and 7-day windows; tap the widget when you want the number right now. A widget's
own `updatePeriodMillis` is capped at 30 min *and* ignored in Doze, which is why
WorkManager (the one real dependency) is here at all.

**Stale data is shown as stale**, same contract as the firmware's idle "Zzz"
screen and the web view's header. A failed fetch keeps the last numbers but the
header says how old they are, rather than presenting an hour-old percentage as
current.

**Cleartext HTTP is enabled globally** in `res/xml/network_security_config.xml`.
Per-host would be better, but Android's `<domain>` rules match hostnames by
suffix with no CIDR support, so "any private address" is not expressible — and
the host isn't known until the user types it at setup. The app only ever GETs
the configured URL and sends no credentials. Moving the service to HTTPS (e.g.
Tailscale Funnel) lets you set this to `false`.

**No server address is baked in.** It's a user's own network layout, so it lives
in SharedPreferences, not in a public repo.

## Layout

```
app/src/main/
  java/com/clawdmeter/widget/
    UsageData.kt          — the short-key payload (same JSON the firmware parses)
    UsageRepository.kt    — HttpURLConnection GET + SharedPreferences cache
    WidgetRenderer.kt     — all the drawing; size-driven layout
    UsageWidgetProvider.kt— AppWidgetProvider: update, resize, tap
    RefreshWorker.kt      — periodic + on-demand refresh
    ConfigActivity.kt     — server address setup
  res/
    font/styrene_b.otf    — copied from assets/, same face as the panel
    drawable-nodpi/logo.png — the 80x80 pixel-art crab
    xml/widget_info.xml   — sizing + resize behaviour
```
