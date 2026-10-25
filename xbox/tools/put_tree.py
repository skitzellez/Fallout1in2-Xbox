# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Copies a whole folder into the Fallout app's storage on the Xbox through Device Portal (test setup without a USB
# stick): put_tree.py <local folder> [console folder under LocalState, default Skitzelles/Fallout1in2]
# Makes every folder, then sends the files many per request. Files already there with the same size are skipped.
import json, os, sys, tempfile, urllib.parse

# xb.py: the Device Portal helper (XB_TOOLS, default: XemuUWP/tools next to the work folder)
sys.path.insert(0, os.environ.get("XB_TOOLS", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "XemuUWP", "tools")))
os.environ["XB_FAMILY"] = "GamerDad.Fallout1in2"
import xb  # noqa: E402

local = os.path.abspath(sys.argv[1])
target = "\\LocalState\\" + (sys.argv[2] if len(sys.argv) > 2 else "Skitzelles\\Fallout1in2").replace("/", "\\")
pfn = xb.newest()["PackageFullName"]
q = lambda path: urllib.parse.quote(path, safe="")
api = xb.B + "/api/filesystem/apps/%s?knownfolderid=LocalAppData&packagefullname=" + pfn


def listing(folder):
    try:
        return {i["Name"].lower(): i for i in json.loads(xb.curl(api % "files" + "&path=" + q(folder))).get("Items", [])}
    except ValueError:
        return {}


tok = xb.token()
top = os.path.basename(local)
if top.lower() not in listing(target):
    xb.curl("-X", "POST", "-H", "X-CSRF-Token: " + tok, "-H", "Content-Length: 0",
            api % "folder" + "&path=" + q(target) + "&newfoldername=" + q(top))
sent = 0
for root, dirs, files in os.walk(local):
    rel = os.path.relpath(root, os.path.dirname(local))
    folder = target + "\\" + rel.replace("/", "\\")
    have = listing(folder)
    for d in dirs:
        if d.lower() not in have:
            r = xb.curl("-X", "POST", "-H", "X-CSRF-Token: " + tok, "-H", "Content-Length: 0",
                        api % "folder" + "&path=" + q(folder) + "&newfoldername=" + q(d), "-w", "%{http_code}")
            if not r.endswith("200"):
                sys.exit("mkdir failed: %s\\%s (%s)" % (folder, d, r))
    todo = [f for f in files if not (f.lower() in have and have[f.lower()].get("FileSize") == os.path.getsize(os.path.join(root, f)))]
    while todo:
        batch, size = [], 0
        while todo and len(batch) < 200 and size < 96 << 20:
            f = todo.pop(0)
            batch.append(f)
            size += os.path.getsize(os.path.join(root, f))
        with tempfile.NamedTemporaryFile("w", suffix=".cfg", delete=False, encoding="utf-8") as cfg:
            for i, f in enumerate(batch):
                cfg.write('form = "f%d=@\\"%s\\""\n' % (i, os.path.join(root, f).replace("\\", "/")))
        r = xb.curl("-K", cfg.name, "-H", "X-CSRF-Token: " + tok, "-H", "Expect:",
                    api % "file" + "&path=" + q(folder), "-w", "%{http_code}", timeout=3600)
        os.unlink(cfg.name)
        if not r.endswith("200"):
            sys.exit("upload failed in %s (%s)" % (folder, r[-200:]))
        sent += len(batch)
        print("%6d files sent  (%s)" % (sent, rel), flush=True)
print("done", sent)
