#!/usr/bin/env python3
"""Update only the owner's two README download URLs from GitHub's latest release.

Versioned filenames remain unchanged. Drafts/prereleases, missing or ambiguous
assets, and unexpected README labels fail without writing the file. All other
README bytes, including its wording, screenshots and line endings, are retained.
"""
import argparse
import json
import os
from pathlib import Path
import re
import urllib.parse
import urllib.request

REPOSITORY = "RLCypher/audiohole"
API = "https://api.github.com/repos/" + REPOSITORY


def latest_release():
    headers = {"Accept": "application/vnd.github+json",
               "User-Agent": "AudioHole-README-download-links"}
    if os.environ.get("GH_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["GH_TOKEN"]
    with urllib.request.urlopen(urllib.request.Request(API + "/releases/latest", headers=headers),
                                timeout=30) as response:
        return json.load(response)


def download_links(release):
    tag = release["tag_name"]
    match = re.fullmatch(r"v?(\d+)\.(\d+)(?:\.(\d+))?", tag)
    if release.get("draft") or release.get("prerelease") or not match:
        raise ValueError("README downloads require a published stable numeric version")
    major, minor, patch = (int(part or 0) for part in match.groups())
    version = (rf"{major}\.{minor}\.{patch}" if patch else rf"{major}\.{minor}(?:\.0)?")
    links = {}
    for label, kind, extension in (("Windows installer", "Installer", "exe"),
                                    ("Portable ZIP", "Portable", "zip")):
        pattern = rf"AudioHole\.{kind}\.v{version}-[0-9a-f]{{7}}\.{extension}"
        assets = [asset for asset in release["assets"]
                  if re.fullmatch(pattern, asset["name"].replace(" ", "."))]
        if len(assets) != 1:
            raise ValueError(f"Latest release needs exactly one {kind} download")
        asset = assets[0]
        url = asset["browser_download_url"]
        parsed = urllib.parse.urlsplit(url)
        expected_path = f"/{REPOSITORY}/releases/download/{tag}/{asset['name']}"
        if (asset.get("state") != "uploaded" or asset.get("size", 0) <= 0 or
                not re.fullmatch(r"sha256:[0-9a-f]{64}", asset.get("digest", "")) or
                parsed.scheme != "https" or parsed.netloc != "github.com" or
                urllib.parse.unquote(parsed.path) != expected_path or parsed.query or parsed.fragment):
            raise ValueError(f"Invalid {kind} release asset or download URL")
        links[label] = url
    return links


def rewrite(readme, links):
    text = readme.decode("utf-8")
    for label, url in links.items():
        pattern = r"(\[" + re.escape(label) + r"\]\()([^\r\n)]+)(\))"
        text, count = re.subn(pattern, lambda match: match[1] + url + match[3], text)
        if count != 1:
            raise ValueError(f"Expected one existing {label!r} README link; found {count}")
    return text.encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--readme", type=Path, default=Path("README.md"))
    parser.add_argument("--check", action="store_true", help="Check links without changing the README")
    args = parser.parse_args()
    release = latest_release()
    links = download_links(release)
    before = args.readme.read_bytes()
    after = rewrite(before, links)
    if args.check and before != after:
        raise SystemExit("README download links do not match " + release["tag_name"])
    if not args.check and before != after:
        args.readme.write_bytes(after)
    print("README downloads point to " + release["tag_name"])
    for label, url in links.items():
        print(label + ": " + url)


if __name__ == "__main__":
    main()
