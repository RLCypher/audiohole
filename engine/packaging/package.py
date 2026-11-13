"""Package an already-built standalone host and its exact module source.

This performs no builds, tests, uploads, repository writes or GUI changes.
Verification is recorded separately; packaging alone never asserts test success.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_files(engine, repository):
    result = []
    for directory, subdirectories, files in os.walk(engine):
        subdirectories[:] = [
            name for name in subdirectories
            if not name.startswith("build") and name not in {"out", "__pycache__", ".git"}
        ]
        result.extend(Path(directory) / name for name in files)
    # Only the validation host needs the existing scanner's read-only store.
    result.extend((repository / "include/audiohole").glob("*.hpp"))
    result.extend(repository / "src/scanner" / name for name in [
        "scanner.cpp", "store.cpp", "atomic_file.cpp", "storage_internal.hpp"
    ])
    result.extend(repository / name for name in [
        "PROFILE_BUILDER.md", "ENGINEERING.md", "AI_DISCLOSURE.md", ".clang-format",
        "docs/test-profile-example.json"
    ])
    return sorted(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--runtime-notices", type=Path)
    parser.add_argument("--version", default="0.6")
    args = parser.parse_args()
    engine = Path(__file__).resolve().parents[1]
    repository = engine.parent
    output = (args.out or engine / "out").resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / f"AudioHole-Engine-Module-{args.version}-source.zip"
    files = source_files(engine, repository)
    manifest = {str(path.relative_to(repository)): digest(path) for path in files}
    with zipfile.ZipFile(source, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.relative_to(repository).as_posix())
        archive.writestr("SOURCE-MANIFEST.json", json.dumps(manifest, indent=2) + "\n")

    binary = output / f"AudioHole-Engine-Validation-{args.version}-windows-x64.zip"
    with tempfile.TemporaryDirectory(prefix="audiohole-engine-package-") as temporary:
        root = Path(temporary)
        subprocess.run([
            args.cmake, "--install", str(args.build.resolve()), "--config", "Release",
            "--prefix", str(root)
        ], check=True)
        executable = root / "audiohole-engine-validation.exe"
        if not executable.is_file():
            raise RuntimeError("Packaging requires a Windows validation executable.")
        header = executable.read_bytes()
        pe = struct.unpack_from("<I", header, 0x3C)[0]
        if header[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", header, pe + 4)[0] != 0x8664:
            raise RuntimeError("Packaging requires a PE x64 executable.")
        (root / "README.md").write_text(
            (engine / "README.md").read_text().replace("../PROFILE_BUILDER.md", "PROFILE_BUILDER.md"),
            encoding="utf-8")
        shutil.copy2(repository / "PROFILE_BUILDER.md", root / "PROFILE_BUILDER_BASELINE.md")
        profile = root / "Profile"
        profile.mkdir()
        (profile / "README.txt").write_text(
            "Put individual audiohole-profile schema-3 JSON files here; v1/v2 remain loadable.\n"
            "Aggregate audiohole-test-configuration v7 payloads are not routing inputs.\n"
            "Run the validation executable, then list / select NUMBER / start / status / stop.\n"
            "No example route or simulated device is loaded automatically.\n", encoding="utf-8")
        (root / "Run-Validation.cmd").write_bytes(
            b'@echo off\r\ncd /d "%~dp0"\r\n"%~dp0audiohole-engine-validation.exe"\r\npause\r\n')
        (root / "Run-Low-Latency.cmd").write_bytes(
            b'@echo off\r\ncd /d "%~dp0"\r\n"%~dp0audiohole-engine-validation.exe" --low-latency\r\npause\r\n')
        if args.runtime_notices:
            for path in args.runtime_notices.iterdir():
                if path.is_file():
                    shutil.copy2(path, root / "licenses" / path.name)
        cache = (args.build / "CMakeCache.txt").read_text()
        backends = ["wasapi", "experimental"]
        if "AUDIOHOLE_ENGINE_WITH_MINIAUDIO:BOOL=ON" in cache:
            backends.insert(1, "miniaudio")
        info = {
            "format": "audiohole-engine-validation-build", "schema_version": 1,
            "version": args.version + "-validation", "platform": "windows-x64",
            "source_baseline": "fda20900e8954993e267e2ca1d6d7be8a19840dc",
            "source_archive": source.name, "source_archive_sha256": digest(source),
            "executable_sha256": digest(executable),
            "backends": backends,
            "automatic_excludes_experimental": True,
            "current_profile_schema_version": 3,
            "accepted_profile_schema_versions": [1, 2, 3],
            "aggregate_transfer_schema_version": 7,
            "aggregate_is_routing_input": False,
            "clock_preference_applied": False,
            "default_latency_preset": "baseline",
            "low_latency_preset": "Opt-in --low-latency; native WASAPI/Experimental only",
            "verification": "Packaging does not assert execution or hardware validation. See VALIDATION.md.",
            "source_files": manifest,
        }
        (root / "BuildInfo.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
        evidence = engine / "VALIDATION.md"
        if evidence.exists():
            shutil.copy2(evidence, root / evidence.name)
        for name in ["CHANGES.md", "LIVE_RUN_REVIEW_2026-11-11.md",
                     "LIVE_RUN_REVIEW_2026-11-11_V02.md", "LIVE_RUN_REVIEW_2026-11-11_LAGMETER.md",
                     "LOW_LATENCY_TEST.md", "STARTUP_BUFFER_TEST.md",
                     "LIVE_RUN_REVIEW_2026-11-11_V03_AB.md",
                     "LIVE_RUN_REVIEW_2026-11-11_WIRED_AB.md", "PROFILE_V3_HANDOFF.md",
                     "LIVE_RUN_REVIEW_2026-11-11_V04_BACKENDS.md",
                     "LIVE_RUN_REVIEW_2026-11-12_EXCLUSIVE.md",
                     "LIVE_RUN_REVIEW_2026-11-12_V051_ACCESS.md",
                     "LIVE_RUN_REVIEW_2026-11-12_V051_BROWSER.md",
                     "STARTUP_PROGRESS_TEST.md"]:
            shutil.copy2(engine / name, root / name)
        with zipfile.ZipFile(binary, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(root).as_posix())
    for path in [source, binary]:
        print(f"{digest(path)}  {path.name} ({path.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
