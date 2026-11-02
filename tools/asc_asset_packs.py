"""Uploads Apple-hosted asset packs (one per Dimension) to App Store Connect.

Usage: asc_asset_packs.py PACK_ID ARCHIVE.aar [PACK_ID ARCHIVE.aar ...]
Env: ASC_KEY_ID, ASC_ISSUER_ID, ASC_KEY_PATH.

Follows "Uploading and versioning Apple hosted background assets": create (or reuse)
the asset pack record, create a new version, reserve an upload, PUT the parts,
commit, then report the processing state. A processed version is ready for
internal TestFlight; external testers and the App Store need review."""
import hashlib, json, os, sys, time, urllib.error, urllib.parse, urllib.request
import jwt

BUNDLE_ID = "com.krowsingre.viksen"
API = "https://api.appstoreconnect.apple.com"
key = open(os.environ["ASC_KEY_PATH"]).read()
kid, iss = os.environ["ASC_KEY_ID"].strip(), os.environ["ASC_ISSUER_ID"].strip()


def token():
    now = int(time.time())
    return jwt.encode({"iss": iss, "iat": now, "exp": now + 1100, "aud": "appstoreconnect-v1"}, key,
                      algorithm="ES256", headers={"kid": kid, "typ": "JWT"})


def call(method, path, body=None, params=None):
    url = API + path + ("?" + urllib.parse.urlencode(params) if params else "")
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={"Authorization": f"Bearer {token()}", "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            raw = r.read()
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        detail = e.read().decode()[:1500]
        raise RuntimeError(f"{method} {path} -> HTTP {e.code}: {detail}") from None


def app_id():
    apps = call("GET", "/v1/apps", params={"filter[bundleId]": BUNDLE_ID})["data"]
    if not apps:
        sys.exit(f"::error::No app with bundle id {BUNDLE_ID} in App Store Connect")
    return apps[0]["id"]


def find_or_create_pack(app, pack_id):
    try:
        existing = call("GET", f"/v1/apps/{app}/backgroundAssets", params={"limit": "200"})["data"]
        for a in existing:
            if a["attributes"].get("assetPackIdentifier") == pack_id:
                return a["id"]
    except RuntimeError as e:
        print(f"(listing asset packs failed, will try to create: {e})")
    body = {"data": {"type": "backgroundAssets", "attributes": {"assetPackIdentifier": pack_id},
                     "relationships": {"app": {"data": {"type": "apps", "id": app}}}}}
    return call("POST", "/v1/backgroundAssets", body)["data"]["id"]


def upload(pack_record, archive):
    version = call("POST", "/v1/backgroundAssetVersions", {"data": {"type": "backgroundAssetVersions", "relationships": {
        "backgroundAsset": {"data": {"type": "backgroundAssets", "id": pack_record}}}}})["data"]
    vid = version["id"]
    print(f"  version record {vid} (version {version['attributes'].get('version', '?')})")
    size = os.path.getsize(archive)
    res = call("POST", "/v1/backgroundAssetUploadFiles", {"data": {"type": "backgroundAssetUploadFiles",
        "attributes": {"assetType": "ASSET", "fileName": os.path.basename(archive), "fileSize": size},
        "relationships": {"backgroundAssetVersion": {"data": {"type": "backgroundAssetVersions", "id": vid}}}}})["data"]
    ops = res["attributes"].get("uploadOperations") or []
    print(f"  uploading {size / 1e6:.1f} MB in {len(ops)} part(s)")
    md5 = hashlib.md5()
    with open(archive, "rb") as f:
        for op in ops:
            f.seek(op["offset"])
            chunk = f.read(op["length"])
            md5.update(chunk)
            headers = {h["name"]: h["value"] for h in op.get("requestHeaders", [])}
            req = urllib.request.Request(op["url"], data=chunk, method=op.get("method", "PUT"), headers=headers)
            for attempt in range(4):
                try:
                    urllib.request.urlopen(req, timeout=600).read()
                    break
                except (urllib.error.URLError, TimeoutError) as e:
                    if attempt == 3:
                        raise
                    print(f"  part at {op['offset']} failed ({e}), retrying")
                    time.sleep(2 ** (attempt + 1))
    call("PATCH", f"/v1/backgroundAssetUploadFiles/{res['id']}", {"data": {"type": "backgroundAssetUploadFiles",
        "id": res["id"], "attributes": {"uploaded": True}}})
    print("  committed; processing")
    for _ in range(60):
        time.sleep(20)
        v = call("GET", f"/v1/backgroundAssetVersions/{vid}")["data"]["attributes"]
        state = v.get("state") or v.get("processingState") or "?"
        print(f"  state: {state}")
        if state not in ("?", "PROCESSING", "UPLOADING", "AWAITING_UPLOAD"):
            return state
    return "still processing (check App Store Connect)"


def main(argv):
    if len(argv) < 2 or len(argv) % 2:
        sys.exit(__doc__)
    app = app_id()
    failures = 0
    for pack_id, archive in zip(argv[0::2], argv[1::2]):
        print(f"Asset pack {pack_id}: {archive}")
        try:
            state = upload(find_or_create_pack(app, pack_id), archive)
            print(f"  -> {state}")
            if "FAIL" in str(state).upper():
                failures += 1
        except RuntimeError as e:
            print(f"::error::{pack_id}: {e}")
            failures += 1
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main(sys.argv[1:])
