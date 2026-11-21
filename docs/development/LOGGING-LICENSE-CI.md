# Logging, storage, first-run licensing and build maintenance

## Diagnostic capture

Start Logging / Stop Logging controls a bounded diagnostic history separately
from operational events. Capture starts off each launch. Notifications, command
receipts, routing, performance observations and tutorial progression keep their
operational event stream. Scanner output always reaches the console. Detailed
scan-stage disk journaling follows the logging toggle; saved device capabilities,
profiles, scan results and probe-history records remain independent.

Both diagnostic and operational histories are bounded to 500 events. Optional
periodic rich engine-status messages and avatar diagnostic capture are disabled
when logging is off. Start/Stop capture markers delimit the retained history.
Errors-only filtering includes routing-failed events.

## Scan-session retention

The scanner worker prunes its sessions directory on initial use and before new
session writes. It retains at most 16 session folders and 512 MiB in total,
removing the oldest completed archives first. The active session is never pruned.
Unrelated directories are counted conservatively but are not deleted.
Optional snapshot/progress writes are skipped when that session alone fills the
budget. Space is reserved for compact terminal metadata, and JSON checkpoints
retain the scanner's checked atomic writes. Permanent device/probe-history data
is outside this retention policy. Limits can be reduced through the internal
options seam for deterministic tests without creating hundreds of megabytes.

## License and appearance

The canonical owner agreement is embedded as RCDATA and remains the installer's
LicenseFile and the distributed AudioHole License.txt. First portable launch shows
a scrollable agreement with the installer's English accept/decline wording and
Start AudioHole. Decline, Cancel and close do not record acceptance or construct
a routing controller. Acceptance is saved as license-acceptance.json beside the
tutorial marker; Replay does not reset it. Smoke hosts never touch this receipt.
An installation registered at the exact executable directory already presented
the agreement through setup, so it avoids a redundant popup.

The standalone portable license dialog resolves Windows appearance directly.
Inno Setup uses modern dynamic styling for its complete wizard, custom pages,
message dialogs and uninstaller. High contrast retains native accessibility
behavior. The installer display filename is Install AudioHole v0.1.exe for 0.1.0;
nonzero patch numbers remain visible for subsequent versions.

## Builds and evidence

Binary download names include the display version, platform, configuration and
seven-character commit ID. Full version, build number and 40-character commit
remain in build-info.json. Matching corresponding source stays a separate
versioned Source artifact, uploaded before binary publication.

Pinned actions/checkout v7.0.1 and actions/upload-artifact v7.0.2 both declare
node24. Their upstream action manifests and release SHAs were checked before
updating the workflow. upload-artifact's archive behavior remains enabled.

Evidence capture/uploads are disabled on ordinary push/PR builds. An explicit
workflow_dispatch capture_evidence input enables them. In-memory GUI rendering
assertions, native smoke tests, ordinary pass/fail logs and installer lifecycle
checks still run; failed package/installer checks print their diagnostic results
to the build log. Source and tested binaries remain required artifacts.

## MSVC unsafe-call warning audit

The successful 0b0870b Release job contained 442 C4996 annotations at 49 distinct
locations, all inside wxWidgets compatibility headers under _deps/wxwidgets-src.
No first-party unsafe-call location was identified. The suggested strcpy_s,
wcscpy_s, strncpy_s, _wdupenv_s and related replacements require capacities,
ownership or return-value contracts not present in the existing wrapper APIs.
Blind substitutions would change upstream APIs or behavior. Those wrappers were
therefore left intact, with compiler diagnostics still enabled. First-party
Windows configuration replacement already uses checked CreateFileW/WriteFile/
FlushFileBuffers rather than the unrelated non-Windows fopen branch.

## Validation

The portable/model suite passed all 306 tests, including logging on/off, live scan
output without capture, oldest-first retention, protected active/unrelated data,
large device sets, small storage budgets and independent license acceptance.
The Windows x64 cross-build linked AudioHole.exe successfully. Distribution-tool
fixtures passed filename/version/metadata checks, runtime allowlisting, installer
colocation, source exclusion, overwrite protection and missing-file rejection.
Tutorial generation, branding integrity and workflow YAML checks also passed.

Cross-compilation does not execute native Windows controls or Inno Setup. The
Debug/Release Actions jobs run the native license/logging smoke assertions,
isolated extracted-package checks and complete installer lifecycle before
publishing their artifacts. Native results belong to that tested commit's run.
