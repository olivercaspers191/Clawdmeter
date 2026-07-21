#!/usr/bin/env bash
# Build the widget APK on this box and serve it for the phone to download.
#
#   ./build-and-serve.sh            # release (signed), serve on :8091
#   ./build-and-serve.sh debug      # debug build instead
#   PORT=9000 ./build-and-serve.sh  # different port
#
# The phone downloads over Tailscale, so this works from anywhere — no cable,
# no Windows, and nothing published publicly. Ctrl-C stops the server; it is
# deliberately ad-hoc rather than a permanent route on the usage service, so
# the always-on surface stays just /usage and the web view.
set -euo pipefail

cd "$(dirname "$0")"

TOOLS="${TOOLS:-$HOME/android-build}"
export JAVA_HOME="${JAVA_HOME:-$TOOLS/jdk}"
export ANDROID_HOME="${ANDROID_HOME:-$TOOLS/sdk}"
export PATH="$JAVA_HOME/bin:$PATH"
GRADLE="${GRADLE:-$TOOLS/gradle-8.9/bin/gradle}"
PORT="${PORT:-8091}"

VARIANT="${1:-release}"
case "$VARIANT" in
  release) TASK=assembleRelease; APK=app/build/outputs/apk/release/app-release.apk ;;
  debug)   TASK=assembleDebug;   APK=app/build/outputs/apk/debug/app-debug.apk ;;
  *) echo "usage: $0 [release|debug]" >&2; exit 2 ;;
esac

echo "==> building $VARIANT"
"$GRADLE" "$TASK" --no-daemon -q

# Stage under a stable name so the phone's download link never changes.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp "$APK" "$STAGE/clawdmeter.apk"

SIZE=$(du -h "$STAGE/clawdmeter.apk" | cut -f1)
# Prefer the Tailscale address: an APK link on the LAN IP only works at home.
IP=$(tailscale ip -4 2>/dev/null | head -1 || true)
[ -n "$IP" ] || IP=$(hostname -I | awk '{print $1}')

cat <<EOF

  Open this on the phone:

      http://$IP:$PORT/clawdmeter.apk     ($SIZE)

  Chrome will ask permission to install unknown apps the first time.
  Ctrl-C here when the download is done.

EOF

cd "$STAGE"
exec python3 -m http.server "$PORT" --bind 0.0.0.0
