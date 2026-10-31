#!/usr/bin/env python3
"""Build the web installer (web/index.html, ESP Web Tools) from a release's
flash image, for GitHub Pages (.github/workflows/pages.yml).

    python3 tools/p4/web_site.py dist/ts2068-p4-v0.1.0.zip [--out _site]

The browser can't fetch a release's files (GitHub sends no CORS headers),
so the site carries the image itself, cut after the last byte that isn't
FFh: what follows is blank flash, and littlefs needs nothing there. The
card stays a link to the release.
"""

import argparse
import json
import os
import re
import shutil
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RELEASES = "https://github.com/timex-sinclair-projects/ts2068-p4/releases"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image_zip", help="ts2068-p4-VERSION.zip")
    ap.add_argument("--out", default="_site")
    a = ap.parse_args()
    version = re.match(r"ts2068-p4-(.+)\.zip$", os.path.basename(a.image_zip)).group(1)
    with zipfile.ZipFile(a.image_zip) as z:
        image = z.read("ts2068-p4-%s.bin" % version)
    end = len(image.rstrip(b"\xff"))
    end = (end + 0xfff) & ~0xfff                # a whole sector

    os.makedirs(a.out, exist_ok=True)
    name = "ts2068-p4-%s.bin" % version
    with open(os.path.join(a.out, name), "wb") as f:
        f.write(image[:end])
    manifest = {
        "name": "TS2068 on ESP32-P4",
        "version": version,
        "new_install_prompt_erase": True,
        "builds": [{"chipFamily": "ESP32-P4", "parts": [{"path": name, "offset": 0}]}],
    }
    with open(os.path.join(a.out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    card = "ts2068-p4-sd-%s.zip" % version
    with open(os.path.join(REPO, "web", "index.html")) as f:
        page = f.read()
    for k, v in (("VERSION", version), ("CARD_NAME", card),
                 ("CARD_URL", "%s/download/%s/%s" % (RELEASES, version, card)),
                 ("RELEASE_URL", "%s/tag/%s" % (RELEASES, version))):
        page = page.replace("{{%s}}" % k, v)
    with open(os.path.join(a.out, "index.html"), "w") as f:
        f.write(page)
    shutil.copy(os.path.join(REPO, "docs", "images", "ts2068-p4.jpg"), a.out)
    print("%s: %s, %d bytes of %d" % (a.out, name, end, len(image)))


if __name__ == "__main__":
    main()
