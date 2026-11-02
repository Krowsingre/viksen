"""Keeps CI from piling up "Apple Development" certificates.

Each TestFlight run starts on a fresh runner with an empty keychain, so Xcode's
automatic signing asks Apple for a new development certificate; Apple caps how
many a team can have. Usage:
  asc_ci_certs.py snapshot FILE   record the certificate ids that exist now
  asc_ci_certs.py cleanup FILE    revoke DEVELOPMENT certificates created since the
                                  snapshot (only those: never older ones, never
                                  distribution certificates)
  asc_ci_certs.py prune           revoke older CI-made DEVELOPMENT certificates
                                  ("Created via API"), owner-approved; never
                                  distribution certificates
Env: ASC_KEY_ID, ASC_ISSUER_ID, ASC_KEY_PATH."""
import json, os, sys, time, urllib.parse, urllib.request, urllib.error
import jwt

key = open(os.environ["ASC_KEY_PATH"]).read()
kid, iss = os.environ["ASC_KEY_ID"].strip(), os.environ["ASC_ISSUER_ID"].strip()


def call(method, path, params=None):
    now = int(time.time())
    token = jwt.encode({"iss": iss, "iat": now, "exp": now + 600, "aud": "appstoreconnect-v1"}, key,
                       algorithm="ES256", headers={"kid": kid, "typ": "JWT"})
    url = "https://api.appstoreconnect.apple.com" + path + ("?" + urllib.parse.urlencode(params) if params else "")
    req = urllib.request.Request(url, method=method, headers={"Authorization": f"Bearer {token}"})
    for attempt in range(4):   # App Store Connect occasionally times out
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                body = r.read()
                return json.loads(body) if body else {}
        except (TimeoutError, urllib.error.URLError) as e:
            if isinstance(e, urllib.error.HTTPError) or attempt == 3:
                raise
            time.sleep(2 ** (attempt + 1))


def certificates():
    return call("GET", "/v1/certificates",
                {"limit": "200", "fields[certificates]": "certificateType,name,displayName,expirationDate"})["data"]


mode = sys.argv[1]
path = sys.argv[2] if len(sys.argv) > 2 else ""
if mode == "prune":
    ci = [c for c in certificates() if c["attributes"].get("certificateType") == "DEVELOPMENT"
          and "Created via API" in ((c["attributes"].get("displayName") or "") + (c["attributes"].get("name") or ""))]
    for c in ci:
        try:
            call("DELETE", f"/v1/certificates/{c['id']}")
            print(f"Revoked old CI development certificate {c['id']} (expires {str(c['attributes'].get('expirationDate'))[:10]})")
        except urllib.error.HTTPError as e:
            print(f"::warning::Could not revoke {c['id']}: HTTP {e.code}")
    print(f"{len(ci)} CI development certificate(s) revoked")
elif mode == "snapshot":
    ids = [c["id"] for c in certificates()]
    json.dump(ids, open(path, "w"))
    print(f"{len(ids)} certificate(s) before this run")
elif mode == "cleanup":
    before = set(json.load(open(path)))
    new = [c for c in certificates() if c["id"] not in before and c["attributes"].get("certificateType") == "DEVELOPMENT"]
    for c in new:
        try:
            call("DELETE", f"/v1/certificates/{c['id']}")
            print(f"Revoked the development certificate this run created ({c['attributes'].get('name', c['id'])})")
        except urllib.error.HTTPError as e:
            print(f"::warning::Could not revoke {c['id']}: HTTP {e.code}")
    if not new:
        print("This run created no development certificate")
else:
    sys.exit("usage: asc_ci_certs.py prune | snapshot FILE | cleanup FILE")
