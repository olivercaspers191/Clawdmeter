#!/usr/bin/env bash
# Bundle the release signing key + its passwords into ONE encrypted file you can
# store anywhere — password manager, private repo, cloud drive, USB stick.
#
#   ./backup-signing-key.sh [output.gpg]
#
# gpg prompts for a passphrase; it is never passed on the command line (that
# would put it in your shell history and in /proc). Pick something you can
# recover from a password manager — this passphrase is now the thing that
# protects the key.
#
# Restore:
#   gpg -d clawdmeter-signing-backup.tar.gpg | tar -xv -C /
#   ...or extract elsewhere and copy the two files back to
#   ~/.android/ and ~/.gradle/ by hand.
#
# Why bother: the key is what lets a new build install *over* the copy already
# on the phone. Without it you must uninstall and re-add the widget — a two
# minute annoyance, not a disaster. Back it up accordingly: one encrypted file,
# two locations, done.
set -euo pipefail

KEYSTORE="$HOME/.android/clawdmeter-release.jks"
GRADLE_PROPS="$HOME/.gradle/gradle.properties"
OUT="${1:-$HOME/clawdmeter-signing-backup.tar.gpg}"

[ -f "$KEYSTORE" ] || { echo "no keystore at $KEYSTORE" >&2; exit 1; }
grep -q '^CLAWD_' "$GRADLE_PROPS" 2>/dev/null || {
  echo "no CLAWD_* properties in $GRADLE_PROPS" >&2; exit 1; }

STAGE="$(mktemp -d)"
chmod 700 "$STAGE"
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/clawdmeter-signing"
cp "$KEYSTORE" "$STAGE/clawdmeter-signing/clawdmeter-release.jks"
# Only the CLAWD_* lines — the rest of gradle.properties is unrelated and may
# hold credentials for other projects that don't belong in this bundle.
grep '^CLAWD_' "$GRADLE_PROPS" > "$STAGE/clawdmeter-signing/gradle-properties-CLAWD.txt"

cat > "$STAGE/clawdmeter-signing/README.txt" <<EOF
Clawdmeter Android release signing key
Created: $(date -u +%Y-%m-%dT%H:%M:%SZ) on $(hostname)

WHAT THIS IS
  clawdmeter-release.jks            the signing key
  gradle-properties-CLAWD.txt       keystore + key passwords, and the alias

WHERE IT GOES
  clawdmeter-release.jks       -> ~/.android/clawdmeter-release.jks
  the CLAWD_* lines            -> append to ~/.gradle/gradle.properties (chmod 600)

WHY IT MATTERS
  Android will only install an update over an existing app if both are signed
  by the same key. Lose this and you must uninstall the widget and re-add it
  (re-entering the server URL). Nothing else is lost — the app itself rebuilds
  from the git repo.

  Anyone holding this key can build an APK that installs over yours. Keep the
  bundle encrypted wherever it is stored.
EOF

chmod -R go-rwx "$STAGE/clawdmeter-signing"

echo "==> encrypting to $OUT"
echo "    (choose a passphrase you can recover from a password manager)"
tar -C "$STAGE" -cf - clawdmeter-signing \
  | gpg --symmetric --cipher-algo AES256 -o "$OUT"
chmod 600 "$OUT"

# Prove the bundle actually restores before trusting it. An unverified backup
# is just a file you feel good about.
echo
echo "==> verifying (enter the same passphrase)"
gpg -d "$OUT" 2>/dev/null | tar -tv

cat <<EOF

Done: $OUT ($(du -h "$OUT" | cut -f1))

Store it in TWO places, e.g.:
  - your password manager (as a file attachment), and
  - a private repo / cloud drive / USB stick

It is encrypted, so the hosting choice is not critical — the passphrase is.
Keep the passphrase somewhere other than next to the file.
EOF
