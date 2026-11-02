#!/usr/bin/env bash
# Downloads Steam Audio (Apache-2.0, Valve) prebuilt SDK into third_party/steamaudio.
# Pinned version + SHA-256; the binaries are not committed to the repository.
set -euo pipefail
VERSION="4.8.1"
SHA256="4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/third_party/steamaudio"
if [ -f "$DEST/.version" ] && [ "$(cat "$DEST/.version")" = "$VERSION" ]; then
  echo "Steam Audio $VERSION already present"; exit 0
fi
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -sSL --retry 3 -o "$TMP/sa.zip" "https://github.com/ValveSoftware/steam-audio/releases/download/v$VERSION/steamaudio_$VERSION.zip"
echo "$SHA256  $TMP/sa.zip" | sha256sum -c - >/dev/null 2>&1 || echo "$SHA256  $TMP/sa.zip" | shasum -a 256 -c -
unzip -q "$TMP/sa.zip" 'steamaudio/include/*' 'steamaudio/lib/ios/*' 'steamaudio/lib/linux-x64/*' \
  'steamaudio/lib/osx/libphonon.dylib' 'steamaudio/lib/windows-x64/phonon.*' 'steamaudio/THIRDPARTY.md' -d "$TMP"
rm -rf "$DEST" && mkdir -p "$DEST"
cp -R "$TMP/steamaudio/." "$DEST/"
curl -sSL --retry 3 -o "$DEST/LICENSE.md" "https://raw.githubusercontent.com/ValveSoftware/steam-audio/v$VERSION/LICENSE.md" || true
echo "$VERSION" > "$DEST/.version"
echo "Steam Audio $VERSION installed in third_party/steamaudio"
