# About/Update, supplied RTF and separate downloads

Settings now opens About/Update beside Restart AudioHole. The dialog preserves
the owner's approved BLANCO paragraphs and wording, without bullets. Version,
seven-character Build and Vintage come from the executable's compiled identity.
Its body scrolls when necessary; View license and Close remain available.

View license opens the same embedded owner-supplied RTF as portable first use,
with a read-only Close action. It inherits the app theme, never changes routing,
edits or license acceptance, and does not reopen onboarding. Portable first use
retains the acceptance gate and resolves the Windows theme. Both viewers use
native Rich Edit; no HTML/web dependency, temporary license file or content
conversion is introduced. Document typography remains intact, with foreground
ink adjusted to the current light/dark/high-contrast palette. Inno Setup consumes
the exact RTF directly. The main-folder license is now AudioHole License.rtf.

The supplied RTF and distribution README.txt are copied unchanged. Git attributes
disable newline conversion for these canonical files. The package manifest,
resource dependency, installer LicenseFile and licensing links use the new name;
the old agreement TXT is removed. Existing installer manifests retire the old
owned filename during update. licenses/GPL-3.0.txt and licenses/assets.txt retain
their separate authoritative terms.

## Build identity

cmake/BuildIdentity.cmake writes build-identity.json during generation. The
executable compiles its version, short commit and English Vintage from that
identity. Packaging checks its full commit/version before writing these added
build-info.json fields (the additive schema remains version 1):

- built_at_utc: ISO 8601 timestamp establishing this build.
- build_date_utc: YYYY-MM-DD from that timestamp.
- vintage: the same date as Month D, YYYY, independent of user locale.
- project_codename: BLANCO.

packaged_at_utc remains the separate packaging time. CI establishes one UTC
timestamp and passes it to both configurations; repackaging does not manufacture
a new Vintage. Local configure uses its UTC configure time unless an explicit
AUDIOHOLE_BUILD_TIMESTAMP is supplied. Packaging rejects a stale commit or an
inconsistent date/label rather than distributing conflicting About metadata.

## Download layout

Download filenames sort in the requested order without numeric prefixes:

1. AudioHole Installer v<display-version>-<short-commit>.exe: Release setup only.
2. AudioHole Portable v<display-version>-<short-commit>.zip: tested Release runtime.
3. AudioHole ZIP-debug v<display-version>-<short-commit>.zip: tested Debug runtime.
4. AudioHole ZIP-source v<display-version>-<short-commit>.zip: matching source.

The source job publishes corresponding source before either binary job can
publish. All four uploads use archive:false, so the installer is a bare EXE and
each ZIP downloads directly without another ZIP wrapper. Debug and Release run
the native GUI/full contract suites; only Release builds/tests setup. The same
filenames appear in additive build-info.json fields installer_file,
portable_archive, debug_archive and source_archive, beside release_repository
and release_tag. Full version and commit remain authoritative.

The manual Prepare unpublished release draft workflow consumes a successful
native main-branch run at the exact checked-out commit. It checks matching clean
source/Debug/Release metadata and Vintage, ZIP manifests, dependency sources and
installer type, then adds SHA256SUMS.txt and release-info.json. It creates only a
draft and verifies GitHub asset digests; it never publishes or changes visibility.

## Validation contracts

Native smoke dispatches About/Update and View license in both app themes, verifies
compiled version/Build/Vintage, URL, Close, read-only RTF typography/theme ink,
and unchanged routing/editor state. First-run smoke retains acceptance/decline
and Windows-theme assertions and now verifies rendered bold headings and the
new license's ending. Distribution fixtures verify canonical document hashes,
compiled date metadata, installer-only output, portable installer/source
exclusion, destination separation, overwrite protection and the runtime allowlist.
They also reject stale commits, inconsistent Vintage values and a packaged
agreement that differs from the installer compiler's source.
Native Windows and Inno execution are performed by CI; cross-builds only compile
the native assertions.

The v0.1.1 asynchronous-check fixture pumps GUI events while waiting for its
worker. Its focused About/Update block pauses the independent controller polling
timer and restores its original interval on exit. This isolates the presentation
commands from unrelated scanner/routing publications in Test Mode; the unchanged
strict revision and editor comparisons still detect any controller mutation by
the dialog. Failure logs report both dialog flags, before/after revisions and
whether the draft changed. Normal application polling is unaffected.

Pre-publication checks passed: 306 portable/model tests, the Windows x64 GUI
cross-build, distribution-tool regressions and a real 744-file runtime package.
The runtime audit confirms both uploaded document hashes and installer exclusion.
Workflow YAML, job dependencies and PowerShell run blocks were checked locally.
