# Preparing a release

The canonical project README is the owner's version on main. Preserve its copy;
only the approved installer/portable links and supplied screenshots are changed.

Windows CI builds the exact commit in Debug and Release, runs native GUI and
contract checks, audits both portable archives and tests the Release installer.
Its four downloads are original files, without another ZIP wrapper:

1. AudioHole Installer v0.1-<commit>.exe
2. AudioHole Portable v0.1-<commit>.zip
3. AudioHole ZIP-debug v0.1-<commit>.zip
4. AudioHole ZIP-source v0.1-<commit>.zip

The seven-character commit is for presentation; full source commit, numeric
version 0.1.0, clean status and Vintage remain in each build's metadata. Patch
releases display all three components, for example v0.1.1. Source includes the
pinned dependency sources and build instructions needed for corresponding source.

## Unpublished draft workflow

1. Put the reviewed release commit on main and let its Windows build finish.
2. Run **Prepare unpublished release draft** on main, supplying that Windows
   build's numeric run ID. No rebuild or new Vintage is manufactured.
3. The workflow checks the exact successful main-branch commit and successful
   source/Debug/Release jobs, then downloads the four original files. ZIPs remain
   intact; an installer ZIP wrapper is rejected.
4. Offline audit checks matching clean source/version/commit/Vintage, runtime
   manifests, separate source/setup and complete dependency source directories.
   It writes SHA256SUMS.txt and release-info.json with the exact sizes/hashes.
5. It creates or finishes an **unpublished draft**, with release notes from
   docs/releases/v<full-version>.md and a full-SHA target. Uploaded GitHub asset
   digests/sizes must match. Published releases and drafts for another commit
   are refused. Repository visibility is never modified.
6. If work is an ancestor of the tested release, fast-forward it to the same
   commit. Independent development is preserved and no force push is used.
   The workflow token avoids starting a duplicate build for that branch sync.

The draft uses the project version as its tag and title: v0.1.1 / AudioHole v0.1.1
for this patch release. The owner edits the main README separately. The release page becomes available to everyone
only when the repository is public and the release has been published.

Publication is a separate owner action. Review the draft and assets first;
publish as a stable latest release (not a prerelease) when ready. GitHub provides
release asset download URLs, SHA-256 digests and sizes. The application reads its
version from tag_name and changelog from the release body; Actions artifacts and
drafts never announce an update. The renamed installer and GitHub's dotted-space
names are recognized. The opt-in weekly startup check needs no private API token
once the repository and release are public. Portable copies retain browser-only
updating; installed copies can download and launch the verified installer.

This initial release is unsigned while Microsoft identity validation is pending.
Preparing a draft neither bypasses validation nor claims a code signature.

## README download links

The Keep README downloads on the latest release workflow runs after release
publication or edits, and can also be run manually. It resolves GitHub's latest
published stable release and updates only the Windows installer and Portable ZIP
URLs on main. Version/commit filenames remain intact; the owner's copy and images
are preserved. Missing, ambiguous or invalid assets prevent any README write.
The bot makes a documentation-only commit without starting another application
build. Draft and prerelease downloads never become the README's latest links.

## Local checks

Run tests/scripts/Test-DistributionTools.ps1 with PowerShell and CMake to verify
names, manifests, metadata, canonical documents and rejected payloads. Run
python -m unittest discover -s tests/scripts -p test_release_preparation.py -v
for offline draft-audit tests. The CTest suite also includes Release.Preparation.
UpdatePolicy tests cover both new and legacy installer names, semantic version
matching, invalid/dirty names, trusted URLs/digests and normal update behavior.
