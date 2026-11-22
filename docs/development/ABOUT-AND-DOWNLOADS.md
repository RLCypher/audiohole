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

Artifact names have numeric prefixes for this presentation order:

1. 01-Installer: Release setup executable only.
2. 02-ZIP: tested Release portable contents, without setup or source.
3. 03-Debug: tested Debug portable contents, without setup or source.
4. 04-Source: one matching archive including pinned dependency sources.

The source preparation job configures the pinned dependencies and publishes
corresponding source before either binary job is allowed to publish. This
preserves source availability while presenting Source last. Debug and Release
still execute native GUI and full contract suites. Only Release builds and tests
the installer. Build-Installer.ps1 writes a separate empty destination and refuses
a packaged RTF that differs from its compiler agreement. GitHub's artifact ZIP
wrapper contains only setup for the Installer download. Portable ZIPs are ready
to extract and run, and include the RTF, new README and all existing guides.

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

Pre-publication checks passed: 306 portable/model tests, the Windows x64 GUI
cross-build, distribution-tool regressions and a real 744-file runtime package.
The runtime audit confirms both uploaded document hashes and installer exclusion.
Workflow YAML, job dependencies and PowerShell run blocks were checked locally.
