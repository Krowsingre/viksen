"""Writes the Background Assets manifest for a Dimension: one pack per Dimension.

Usage: asset_pack_manifests.py world.json PACK_ROOT
Prints the pack id (the Dimension id); writes PACK_ROOT/<pack>.manifest.json.
Owner, 2026-11-05: a Dimension downloads whole; its songs are discovered by listening.
(App Store Connect allows letters, digits and hyphens in pack ids.)
Files live under Dimensions/<dimension>/."""
import json, os, sys

world = json.load(open(sys.argv[1]))
root = sys.argv[2]
dim = world["id"]
songs = world.get("songs") or [world]
files = sorted({f"Dimensions/{dim}/{st['file']}" for s in songs for st in s["stems"]})
missing = [f for f in files if not os.path.exists(os.path.join(root, f))]
if missing:
    sys.exit(f"{dim}: missing {missing}")
json.dump({"assetPackID": dim, "downloadPolicy": {"onDemand": {}},
           "fileSelectors": [{"file": f} for f in files], "platforms": ["iOS"]},
          open(os.path.join(root, f"{dim}.manifest.json"), "w"), indent=2)
print(dim)
