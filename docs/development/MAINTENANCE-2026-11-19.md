# November 19 maintenance batch

## Changes

- Plug Board drawer-open requests are idempotent. Repeated tutorial guidance no
  longer restarts the drawer animation or takes focus off Add socket. GUI smoke
  asserts focus and animation state before committing exactly one socket.
- Tutorial outlines and reply guidance use host-owned, non-topmost, non-activating
  native windows. Deactivation hides every overlay; foreground restoration brings
  guidance back. GUI smoke activates a separate window to verify this boundary.
- The supplied Norm/Horu Bother revision is imported verbatim: 35 responses each,
  categorized 10 general / 10 tips / 5 easter eggs / 10 invented facts. The bounded
  loader limit is 64 per pool. C004 uses the supplied supervisory-position wording.
- Onboarding moves into the existing working data root alongside devices, profiles
  and Plug Board. Legacy executable-side status is copied once without writing to
  the installation. Completion/skipping and greeting receipts are preserved; the
  migration receipt prevents Replay from importing the old marker again.
- Generated docs/build-info.json now contains application version, numeric CI build
  number (0 for local builds), source commit, schema identity, runtime file manifest
  and matching source archive name. The version comes from CMake, with no separate
  version.txt. Runtime packaging retains the strict allowlist and overwrite guard.
- The optional Inno Setup installer consumes that audited ZIP, reads installed
  version metadata, offers Update/Reinstall/Uninstall/Cancel, displays authored
  notes and supports selectable destinations and shortcuts. Update preserves data;
  uninstall defaults to retaining it. Details and limitations are in INSTALLER.md.
- First-party code uses GPL-3.0-only. LICENSE, contribution/DCO guidance, licensing
  scope, dependency notices and corresponding-source build instructions are added.
  CI supplies matching source with all five pinned CMake dependency sources beside
  the binary artifacts. Artwork/branding licensing remains separately deferred.
- Automatic assessment labels mono capture feeding stereo `1 ch (dual mono)`,
  retains native widths and ignores inactive overrides before filtering. Explicit
  requirements/maps remain authoritative. All four mono/stereo combinations are
  checked. The engine still selects Left for automatic 2->1: true stereo-to-mono
  mixing must be implemented by its owner before `1 ch (Mono)` is advertised. This
  batch does not modify engine processing or JSON preference semantics.
- Detailed root documents move to docs/specifications with updated links, install
  rules and indexes. The root retains build files and short entry documents, and
  the README provides a concise introduction and navigation.

## Local verification

- Portable build succeeded; 297 tests ran. One avatar idle-load timeout under the
  simultaneous cross-build passed on an isolated rerun. No assertion was weakened.
- Windows cross-build succeeded. All 51 focused Windows model tests passed,
  covering format assessment, onboarding/workflow, tutorial and response metadata.
- Four Windows GUI modes passed under Wine with fresh PASS evidence: Tutorial,
  main GUI, Test Mode and engine adapter. Wine timings were 71.08 / 92.51 / 39.01 /
  19.09 seconds using a local 120-second watchdog. Native CI timeouts are unchanged.
- Distribution-tool safeguard fixtures passed, including missing/unexpected file
  rejection, installer/source metadata and package overwrite protection.
- Source archive generated and inspected for first-party code, installer/build
  scripts, LICENSE and all five dependency source trees, without host build output.
- All 70 response texts/expressions/fallbacks match the supplied revision exactly.
  Generated tutorial data, entry-point/document links, PowerShell parsing, module
  adaptation hashes and whitespace checks passed.

Native MSVC Debug/Release, native restart and the installer lifecycle are verified
by the Windows workflow after publication. Wine lacks a 32-bit guest subsystem,
which the upstream installer engine requires even for its 64-bit build; local
installer execution is therefore unavailable. This is not native installation or
hardware validation. Check the CI result for the published revision; do not infer
native success solely from this local record.

## Native installer correction

The rerun of a0f69f9 completed MSVC Debug/Release builds, all native GUI/restart,
engine/scanner/model checks and extracted-package validation. Installer compilation
and initial installation/update/reinstall checks also passed. The final lifecycle
check failed because it assumed unins000.exe after removal and reinstallation.
The Debug setup log explicitly records unins001.dat and unins001.exe on that pass.

Both the installer Uninstall action and the lifecycle test now resolve the current
UninstallString from the matching installation's Windows registration. The path
must refer to an existing numbered Inno uninstaller inside the selected directory;
the installer also rejects redirected paths. The lifecycle test checks registration
after every setup, exercises the installer's resolver on another reinstall and
records both uninstall logs. A local PowerShell check passed for 000/001/123,
spaces/quoted arguments, and rejection of wrong directories, wrong filenames and
missing files. Native verification of this correction is performed by the next CI
run; the evidence above does not claim that the corrected lifecycle has run yet.

The next native Debug run passed registered-uninstaller resolution, including the
additional reinstall. It exposed a separate test wait bug: Inno's launcher exited
while its temporary second-phase worker still held uninstall-final.log open.
Finally-block cleanup then masked the earlier incomplete-uninstall check with a
sharing-violation error. Run-Uninstall now waits within a 60-second deadline for
actual worker success, a log-closed record and an exclusive log read before any
state assertions or cleanup. Final removal of AudioHole.exe is also asserted.
Cleanup errors are reported without replacing a primary failure. A local delayed-
worker test verified that early launcher exit does not complete the check and that
a failed worker is rejected. Native verification remains assigned to the next run.
