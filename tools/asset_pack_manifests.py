"""Writes one Background Assets manifest per track of a Dimension.

Usage: asset_pack_manifests.py world.json PACK_ROOT
Prints the pack ids, one per line; writes PACK_ROOT/<pack>.manifest.json for each.
Single-song Dimensions: one pack named after the Dimension. Multi-song Dimensions:
one pack per song, named "<dimension>.<song>". Files live under Dimensions/<dimension>/."""
import json, os, sys

world = json.load(open(sys.argv[1]))
root = sys.argv[2]
dim = world["id"]
songs = world.get("songs")
packs = [(dim, world["stems"])] if not songs else [(f"{dim}.{s['id']}", s["stems"]) for s in songs]
for pack, stems in packs:
    files = [f"Dimensions/{dim}/{st['file']}" for st in stems]
    missing = [f for f in files if not os.path.exists(os.path.join(root, f))]
    if missing:
        sys.exit(f"{pack}: missing {missing}")
    json.dump({"assetPackID": pack, "downloadPolicy": {"onDemand": {}},
               "fileSelectors": [{"file": f} for f in files], "platforms": ["iOS"]},
              open(os.path.join(root, f"{pack}.manifest.json"), "w"), indent=2)
    print(pack)
