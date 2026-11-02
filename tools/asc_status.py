"""Prints the App Store Connect / TestFlight state of recent builds.
Needs env ASC_KEY_ID, ASC_ISSUER_ID and the .p8 path in ASC_KEY_PATH.
Read-only: it never changes anything in App Store Connect."""
import json, os, sys, time, urllib.parse, urllib.request, urllib.error
import jwt

BUNDLE_ID = "com.krowsingre.viksen"
key = open(os.environ["ASC_KEY_PATH"]).read()
kid, iss = os.environ["ASC_KEY_ID"].strip(), os.environ["ASC_ISSUER_ID"].strip()


def get(path, params=None):
    now = int(time.time())
    token = jwt.encode({"iss": iss, "iat": now, "exp": now + 600, "aud": "appstoreconnect-v1"}, key,
                       algorithm="ES256", headers={"kid": kid, "typ": "JWT"})
    url = "https://api.appstoreconnect.apple.com" + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, headers={"Authorization": f"Bearer {token}"})
    try:
        return json.load(urllib.request.urlopen(req, timeout=30))
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code} for {path}: {e.read().decode()[:300]}")
        return {"data": []}


apps = get("/v1/apps", {"filter[bundleId]": BUNDLE_ID})["data"]
if not apps:
    sys.exit("No app record for " + BUNDLE_ID)
app = apps[0]
print(f"App: {app['attributes']['name']} ({app['id']})\n")

builds = get("/v1/builds", {"filter[app]": app["id"], "sort": "-uploadedDate", "limit": "10",
                            "include": "buildBetaDetail,preReleaseVersion"})
included = {(i["type"], i["id"]): i for i in builds.get("included", [])}
print(f"{'build':>6} {'version':>8} {'processing':<12} {'internal TF':<26} {'expired':<7} uploaded")
for b in builds["data"]:
    a = b["attributes"]
    rel = b.get("relationships", {})
    det = rel.get("buildBetaDetail", {}).get("data")
    pre = rel.get("preReleaseVersion", {}).get("data")
    internal = included.get(("buildBetaDetails", det["id"]), {}).get("attributes", {}).get("internalBuildState", "?") if det else "?"
    version = included.get(("preReleaseVersions", pre["id"]), {}).get("attributes", {}).get("version", "?") if pre else "?"
    print(f"{a.get('version'):>6} {version:>8} {a.get('processingState', '?'):<12} {internal:<26} "
          f"{str(a.get('expired')):<7} {a.get('uploadedDate', '')[:19]}")
    if a.get("usesNonExemptEncryption") is None:
        print("        export compliance: not answered")

groups = get(f"/v1/apps/{app['id']}/betaGroups", {"limit": "20"})["data"]
print("\nTestFlight groups:")
if not groups:
    print("  none - create an Internal Testing group and add yourself as a tester")
for g in groups:
    ga = g["attributes"]
    gb = get(f"/v1/betaGroups/{g['id']}/builds", {"limit": "5"})["data"]
    testers = get(f"/v1/betaGroups/{g['id']}/betaTesters", {"limit": "20"})["data"]
    print(f"  {ga['name']} ({'internal' if ga.get('isInternalGroup') else 'external'}): "
          f"{len(testers)} tester(s), builds: {[x['attributes']['version'] for x in gb] or 'none'}")
