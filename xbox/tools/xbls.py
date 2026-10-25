# Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
# PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
# Lists a folder in the Fallout app's storage on the Xbox: xbls.py <path under LocalState, / separated>
import json, os, sys, urllib.parse
# xb.py: the Device Portal helper (XB_TOOLS, default: XemuUWP/tools next to the work folder)
sys.path.insert(0, os.environ.get("XB_TOOLS", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "XemuUWP", "tools")))
os.environ["XB_FAMILY"] = "GamerDad.Fallout1in2"
sys.argv, args = sys.argv[:1], sys.argv[1:]
import xb  # noqa: E402
pfn = xb.newest()["PackageFullName"]
path = "/LocalState/" + (args[0] if args else "")
path = path.rstrip("/").replace("/", chr(92))
r = xb.curl(xb.B + "/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname=" + pfn + "&path=" + urllib.parse.quote(path, safe=""))
try:
    for i in json.loads(r).get("Items", []):
        print("%12s  %s" % (i.get("FileSize", "<dir>"), i["Name"]))
except ValueError:
    print("error:", r[:300])
