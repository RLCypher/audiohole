#!/usr/bin/env python3
"""Audit matching Windows artifacts and prepare an explicitly unpublished release.

`check-run` verifies native CI provenance. `prepare` is offline and reads ZIP
metadata without extracting or executing files. `draft` uploads only audited
assets and refuses published releases or a draft targeting another commit.
No command changes repository visibility or publishes a release.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import urllib.error
import urllib.request
import zipfile

REPOSITORY = "RLCypher/audiohole"
API = "https://api.github.com/repos/" + REPOSITORY
KINDS = (("Installer", ".exe"), ("Portable", ".zip"),
         ("ZIP-debug", ".zip"), ("ZIP-source", ".zip"))


def git_commit():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()


def project_version():
    text = Path("CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(AudioHole VERSION (\d+\.\d+\.\d+) ", text)
    if not match:
        raise ValueError("Cannot read AudioHole version")
    return match[1]


def display_version(version):
    return version.removesuffix(".0") if version.endswith(".0") else version


def download_names(version, commit):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Expected the full source commit")
    return [f"AudioHole {kind} v{display_version(version)}-{commit[:7]}{ext}"
            for kind, ext in KINDS]


def github(path, payload=None):
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "AudioHole-release-preparation"}
    token = os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = "Bearer " + token
    data = None
    if payload is not None:
        data = json.dumps(payload).encode()
        headers["Content-Type"] = "application/json"
    request = urllib.request.Request(API + path, data=data, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def check_run(run_id):
    if not re.fullmatch(r"[1-9][0-9]*", run_id):
        raise ValueError("Expected a numeric GitHub Actions build run ID")
    run = github("/actions/runs/" + run_id)
    if (run["head_sha"] != git_commit() or run["head_branch"] != "main"
            or run["event"] not in ("push", "workflow_dispatch")
            or run["path"] != ".github/workflows/windows.yml"
            or run["status"] != "completed" or run["conclusion"] != "success"):
        raise ValueError("Build must be a successful native main-branch run of this exact commit")
    jobs = github(f"/actions/runs/{run_id}/jobs?per_page=100")["jobs"]
    required = {"Prepare matching source", "build-and-test (debug)", "build-and-test (release)"}
    successful = {job["name"] for job in jobs if job["conclusion"] == "success"}
    if not required <= successful:
        raise ValueError("Matching source, Debug and Release checks must all pass")
    print("Verified native build: " + run["html_url"])


def zip_document(path, relative):
    """Read one bounded metadata record; reject unsafe or ambiguous ZIP layouts."""
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        for entry in entries:
            parts = PurePosixPath(entry.filename).parts
            if (not parts or entry.filename.startswith("/") or ".." in parts
                    or "\\" in entry.filename or ":" in entry.filename):
                raise ValueError("Unsafe ZIP member in " + path.name)
        roots = {entry.filename.split("/")[0] for entry in entries}
        if len(roots) != 1:
            raise ValueError("Expected one package root in " + path.name)
        root = next(iter(roots))
        names = [entry.filename for entry in entries if not entry.is_dir()]
        if len(names) != len(set(names)):
            raise ValueError("Duplicate ZIP members in " + path.name)
        member = root + "/" + relative
        info = archive.getinfo(member)
        if info.file_size > 128 * 1024:
            raise ValueError("Oversized build metadata in " + path.name)
        return json.loads(archive.read(info)), {name[len(root) + 1:] for name in names}


def build_identity(metadata):
    # PowerShell can serialize the same UTC instant as Z or +00:00. Compare
    # instants while retaining the separately authored date/Vintage labels.
    timestamp = datetime.fromisoformat(metadata["built_at_utc"].replace("Z", "+00:00"))
    if timestamp.tzinfo is None:
        raise ValueError("Build timestamp must include its UTC offset")
    return timestamp, metadata["build_date_utc"], metadata["vintage"]


def audit_assets(directory, version, commit):
    """Require all four clean, same-commit assets and their matching source."""
    names = download_names(version, commit)
    files = sorted(path.name for path in directory.iterdir() if path.is_file())
    if sorted(names) != files or any(path.is_dir() for path in directory.iterdir()):
        raise ValueError("Expected exactly the four named downloads for this version and commit")
    installer, portable, debug, source = [directory / name for name in names]
    with installer.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError("Installer is not a Windows executable")
    identity = None
    for path, configuration in ((portable, "Release"), (debug, "Debug")):
        metadata, members = zip_document(path, "docs/build-info.json")
        if (metadata.get("format") != "audiohole-build-info" or metadata.get("schema_version") != 1
                or metadata.get("product") != "AudioHole" or metadata.get("configuration") != configuration
                or metadata.get("source_dirty") is not False or metadata.get("commit") != commit
                or metadata.get("version") != version or metadata.get("release_tag") != "v" + version
                or metadata.get("release_repository") != REPOSITORY):
            raise ValueError("Mismatching or dirty build identity in " + path.name)
        for key, name in zip(("installer_file", "portable_archive", "debug_archive", "source_archive"), names):
            if metadata.get(key) != name:
                raise ValueError("Mismatching download metadata: " + key)
        if (set(metadata.get("runtime_files", [])) != members
                or "AudioHole.exe" not in members or "licenses/GPL-3.0.txt" not in members
                or any(name in members for name in names)):
            raise ValueError("Runtime manifest differs or includes installer/source content")
        this_identity = build_identity(metadata)
        if not all(this_identity) or (identity is not None and this_identity != identity):
            raise ValueError("Debug/Release Vintage does not match")
        identity = this_identity
    metadata, members = zip_document(source, "source-build-info.json")
    if (metadata.get("source_dirty") is not False or metadata.get("commit") != commit
            or metadata.get("version") != version
            or build_identity(metadata) != identity):
        raise ValueError("Corresponding source does not match the binaries")
    for dependency in ("wxwidgets", "nlohmann_json", "miniz", "miniaudio", "googletest"):
        if not any(name.startswith("dependencies/" + dependency + "/") for name in members):
            raise ValueError("Missing corresponding dependency source: " + dependency)
    if not {"LICENSE", "CMakeLists.txt", "BUILDING.md"} <= members:
        raise ValueError("Source archive lacks license or build instructions")
    return names


def prepare(directory, output, run_id):
    version, commit = project_version(), git_commit()
    names = audit_assets(directory, version, commit)
    output.mkdir(parents=True, exist_ok=False)
    assets = []
    for name in names:
        path = directory / name
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        assets.append({"name": name, "sha256": digest, "bytes": path.stat().st_size})
        shutil.copyfile(path, output / name)
    manifest = {"format": "audiohole-release-info", "schema_version": 1,
                "repository": REPOSITORY, "version": version, "tag": "v" + version,
                "commit": commit, "actions_run_id": run_id, "signed": False, "assets": assets}
    (output / "release-info.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / "SHA256SUMS.txt").write_text(
        "".join(f"{asset['sha256']}  {asset['name']}\n" for asset in assets), encoding="utf-8")
    print("Audited installer, portable, debug and corresponding source; checksums prepared")


def draft(directory):
    metadata = json.loads((directory / "release-info.json").read_text(encoding="utf-8"))
    version, commit = project_version(), git_commit()
    if (metadata["version"] != version or metadata["commit"] != commit
            or metadata["repository"] != REPOSITORY or metadata["tag"] != "v" + version):
        raise ValueError("Release manifest belongs to another checkout")
    if ([asset["name"] for asset in metadata["assets"]] != download_names(version, commit)):
        raise ValueError("Release manifest must identify exactly the four audited downloads")
    check_run(metadata["actions_run_id"])
    for asset in metadata["assets"]:
        if asset["name"] not in download_names(version, commit):
            raise ValueError("Unexpected release asset")
        path = directory / asset["name"]
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if path.stat().st_size != asset["bytes"] or digest != asset["sha256"]:
            raise ValueError("Release asset changed after audit")
    tag = metadata["tag"]
    try:
        release = github("/releases/tags/" + tag)
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
        release = None
    notes = Path(f"docs/releases/v{version}.md").read_text(encoding="utf-8")
    if release is not None:
        if not release["draft"] or release["target_commitish"] != commit:
            raise ValueError("Refusing to replace a published release or a different release commit")
    else:
        release = github("/releases", {"tag_name": tag, "target_commitish": commit,
                         "name": "AudioHole v" + display_version(version), "body": notes,
                         "draft": True, "prerelease": False})
    # GitHub's CLI streams large asset uploads and handles its upload host.
    # Contents-write credentials belong only to the manually dispatched draft job.
    uploads = [directory / asset["name"] for asset in metadata["assets"]]
    uploads += [directory / "SHA256SUMS.txt", directory / "release-info.json"]
    subprocess.run(["gh", "release", "upload", tag, "--repo", REPOSITORY, "--clobber",
                    *map(str, uploads)], check=True)
    verified = github("/releases/" + str(release["id"]))
    by_name = {asset["name"]: asset for asset in verified["assets"]}
    for asset in metadata["assets"]:
        normalized = asset["name"].replace(" ", ".")
        uploaded = by_name.get(asset["name"]) or by_name.get(normalized)
        if (not uploaded or uploaded["size"] != asset["bytes"]
                or uploaded.get("digest") != "sha256:" + asset["sha256"]):
            raise ValueError("Uploaded asset digest/size mismatch: " + asset["name"])
    if not verified["draft"]:
        raise ValueError("Release unexpectedly ceased to be a draft")
    print("Prepared UNPUBLISHED draft: " + verified["html_url"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check-run", "prepare", "draft"))
    parser.add_argument("--run-id")
    parser.add_argument("--assets", type=Path, default=Path("build/release-input"))
    parser.add_argument("--output", type=Path, default=Path("build/release"))
    args = parser.parse_args()
    if args.command == "check-run":
        check_run(args.run_id or "")
    elif args.command == "prepare":
        if not re.fullmatch(r"[1-9][0-9]*", args.run_id or ""):
            parser.error("prepare requires a numeric --run-id")
        prepare(args.assets, args.output, args.run_id)
    else:
        draft(args.output)


if __name__ == "__main__":
    main()
