#!/usr/bin/env python3
"""Refresh only an unpublished installer's presentation, retaining tested binaries.

The source ZIP retains its clean application baseline and records a separate
committed installer overlay. Existing strict archive audits still apply. Release
copy, README, portable/Debug ZIPs, application build identity and tag target stay
unchanged. Native installer validation belongs to the calling Windows workflow.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

import PrepareRelease as release_tools

ROOT = Path("build/installer-refresh")
ALLOWED = {
    "installer/AudioHole.iss", "installer/ReleaseNotes.txt",
    ".github/workflows/refresh-installer.yml", "scripts/RefreshDraftInstaller.py",
    "docs/development/INSTALLER.md",
}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def live_release(release_id):
    release = release_tools.github("/releases/" + str(release_id))
    if not release["draft"]:
        raise ValueError("Refusing to change a published release")
    return release


def asset_for(release, name):
    return next(asset for asset in release["assets"]
                if asset["name"] in (name, name.replace(" ", ".")))


def fetch():
    version, head = release_tools.project_version(), release_tools.git_commit()
    release = next(r for r in release_tools.github("/releases?per_page=100")
                   if r["tag_name"] == "v" + version)
    release = live_release(release["id"])
    baseline = release["target_commitish"]
    subprocess.run(["git", "merge-base", "--is-ancestor", baseline, head], check=True)
    changed = subprocess.check_output(
        ["git", "diff", "--name-only", baseline, head], text=True).splitlines()
    if not changed or not set(changed) <= ALLOWED:
        raise ValueError("Installer-only refresh cannot change application/build sources: " +
                         repr(changed))
    if subprocess.check_output(["git", "status", "--porcelain"], text=True).strip():
        raise ValueError("Installer source checkout must be clean")
    inputs = ROOT / "input"
    inputs.mkdir(parents=True, exist_ok=False)
    names = release_tools.download_names(version, baseline)
    command = ["gh", "release", "download", release["tag_name"], "--repo",
               release_tools.REPOSITORY, "--dir", str(inputs)]
    for name in names + ["release-info.json", "SHA256SUMS.txt"]:
        command += ["--pattern", asset_for(release, name)["name"]]
    subprocess.run(command, check=True)
    for name in names:
        downloaded = inputs / asset_for(release, name)["name"]
        if downloaded.name != name:
            downloaded.rename(inputs / name)
    metadata = json.loads((inputs / "release-info.json").read_text(encoding="utf-8"))
    if (metadata["commit"] != baseline or metadata["version"] != version or
            metadata["tag"] != release["tag_name"] or
            metadata["repository"] != release_tools.REPOSITORY):
        raise ValueError("Draft metadata does not match its tested application baseline")
    if [a["name"] for a in metadata["assets"]] != names:
        raise ValueError("Draft must contain the four expected downloads")
    for item in metadata["assets"]:
        asset = asset_for(release, item["name"])
        path = inputs / item["name"]
        if (path.stat().st_size != item["bytes"] or digest(path) != item["sha256"] or
                asset["size"] != item["bytes"] or
                asset.get("digest") != "sha256:" + item["sha256"]):
            raise ValueError("Draft download changed: " + item["name"])
    # audit_assets expects just the four downloads, so use a separate directory.
    audit = ROOT / "baseline"
    audit.mkdir()
    for name in names:
        os.link(inputs / name, audit / name)
    release_tools.audit_assets(audit, version, baseline)
    save(ROOT / "context.json", {
        "release_id": release["id"], "tag": release["tag_name"],
        "baseline": baseline, "installer_commit": head, "version": version,
        "changed": changed, "names": names,
        "asset_fingerprint": {a["name"]: [a["id"], a.get("digest"), a["size"]]
                              for a in release["assets"]},
        "portable": str((inputs / names[1]).resolve()),
    })
    print("Verified existing draft; application and Debug binaries will be reused")


def prepare(context):
    names = context["names"]
    inputs, output = ROOT / "input", ROOT / "output"
    output.mkdir(exist_ok=False)
    shutil.copyfile(ROOT / "setup" / names[0], output / names[0])
    for name in names[1:3]:
        os.link(inputs / name, output / name)
    overlay = {path: digest(Path(path)) for path in context["changed"]}
    provenance = {"application_commit": context["baseline"],
                  "installer_commit": context["installer_commit"],
                  "installer_actions_run_id": os.environ["GITHUB_RUN_ID"],
                  "installer_refresh_files": overlay}
    # Preserve every baseline source/dependency member, replacing only the
    # explicitly committed installer/tooling files and bounded metadata record.
    with zipfile.ZipFile(inputs / names[3]) as source:
        metadata, _ = release_tools.zip_document(inputs / names[3], "source-build-info.json")
        metadata.update(provenance)
        root = source.infolist()[0].filename.split("/")[0]
        replacements = {root + "/" + path: Path(path).read_bytes() for path in overlay}
        replacements[root + "/source-build-info.json"] = (
            json.dumps(metadata, indent=2) + "\n").encode()
        with zipfile.ZipFile(output / names[3], "w", compression=zipfile.ZIP_DEFLATED) as target:
            for member in source.infolist():
                if member.filename in replacements:
                    target.writestr(member, replacements.pop(member.filename))
                else:
                    with source.open(member) as incoming, target.open(member, "w") as outgoing:
                        shutil.copyfileobj(incoming, outgoing)
            for name, data in replacements.items():
                target.writestr(name, data)
    release_tools.audit_assets(output, context["version"], context["baseline"])
    with zipfile.ZipFile(output / names[3]) as archive:
        for path, expected in overlay.items():
            if hashlib.sha256(archive.read(root + "/" + path)).hexdigest() != expected:
                raise ValueError("Installer source overlay mismatch: " + path)
    metadata = json.loads((inputs / "release-info.json").read_text(encoding="utf-8"))
    metadata.update(provenance)
    for item in metadata["assets"]:
        path = output / item["name"]
        item.update(bytes=path.stat().st_size, sha256=digest(path))
    save(output / "release-info.json", metadata)
    (output / "SHA256SUMS.txt").write_text(
        "".join(f"{a['sha256']}  {a['name']}\n" for a in metadata["assets"]), encoding="utf-8")
    print("Audited rebuilt installer, exact corresponding source overlay and updated checksums")


def upload(context):
    release = live_release(context["release_id"])
    current = {a["name"]: [a["id"], a.get("digest"), a["size"]] for a in release["assets"]}
    if release["target_commitish"] != context["baseline"] or current != context["asset_fingerprint"]:
        raise ValueError("Draft changed during the installer rebuild; refusing to overwrite it")
    names = context["names"]
    uploads = [names[0], names[3], "release-info.json", "SHA256SUMS.txt"]
    subprocess.run(["gh", "release", "upload", context["tag"], "--repo",
                    release_tools.REPOSITORY, "--clobber",
                    *[str(ROOT / "output" / name) for name in uploads]], check=True)
    release = live_release(context["release_id"])
    metadata = json.loads((ROOT / "output/release-info.json").read_text(encoding="utf-8"))
    for name in uploads:
        path = ROOT / "output" / name
        asset = asset_for(release, name)
        if asset["size"] != path.stat().st_size or asset.get("digest") != "sha256:" + digest(path):
            raise ValueError("Uploaded draft asset failed verification: " + name)
    for name in names[1:3]:
        asset = asset_for(release, name)
        if [asset["id"], asset.get("digest"), asset["size"]] != context["asset_fingerprint"][asset["name"]]:
            raise ValueError("Application/Debug asset changed unexpectedly")
    if metadata["installer_commit"] != context["installer_commit"]:
        raise ValueError("Installer provenance changed unexpectedly")
    print("Refreshed UNPUBLISHED draft, preserving its copy and application downloads: " + release["html_url"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("fetch", "prepare", "upload"))
    args = parser.parse_args()
    if args.command == "fetch":
        fetch()
    else:
        context = json.loads((ROOT / "context.json").read_text(encoding="utf-8"))
        if context["installer_commit"] != release_tools.git_commit():
            raise ValueError("Checkout changed during installer refresh")
        (prepare if args.command == "prepare" else upload)(context)
