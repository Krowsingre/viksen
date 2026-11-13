#!/usr/bin/env bash
# Copies the apps' artwork (every image: asset catalogue image sets, app icons, Windows and
# desktop images) from the private assets repository into this checkout. Run it once after
# cloning, and before building any app. The artwork is never committed here (.gitignore).
#   tools/fetch_art.sh                    clone it (uses $ASSETS_TOKEN in CI, else your git login)
#   tools/fetch_art.sh ../viksen-assets   copy from an existing checkout
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
src="${1:-}"
if [ -z "$src" ]; then
  tmp="$(mktemp -d)"
  trap 'rm -rf "$tmp"' EXIT
  url="https://github.com/Krowsingre/viksen-assets.git"
  if [ -n "${ASSETS_TOKEN:-}" ]; then url="https://x-access-token:${ASSETS_TOKEN}@github.com/Krowsingre/viksen-assets.git"; fi
  git clone -q --depth 1 --filter=blob:none --sparse "$url" "$tmp/assets"
  git -C "$tmp/assets" sparse-checkout set art
  src="$tmp/assets"
fi
[ -d "$src/art" ] || { echo "no art/ folder in $src" >&2; exit 1; }
cp -R "$src/art/." "$root/"
[ -f "$root/apps/ios/Viksen/Resources/Assets.xcassets/AppIcon.appiconset/Contents.json" ] || { echo "artwork incomplete" >&2; exit 1; }
echo "artwork copied: $(find "$src/art" -type f | wc -l | tr -d ' ') files"
