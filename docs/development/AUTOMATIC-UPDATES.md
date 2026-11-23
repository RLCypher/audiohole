# Automatic updates and error visibility

Settings offers an opt-in Check for updates automatically checkbox, off for old
and new configurations. Its hovertext is: "We'll let you know if anything new
comes to this mess." Enabling takes effect on the next normal app startup.
Tutorial and every smoke host perform no real update request or installer launch.

A GUI-owned policy store lives at updates.json beside preferences.json in the
shared local data root. Its format is audiohole-update-state, schema_version 1:
last_checked_unix is a nonnegative UTC Unix timestamp, skipped_version is an empty
or semantic version string, and optional cached_release contains version, notes,
and optional installer {url, sha256, bytes}. Missing files start empty. Malformed
or future schemas are retained and reported in the diagnostics CLI; updates stop
until the state is corrected. Atomic compare-before-replace writes preserve
external edits and unknown root fields. Existing preferences gain only the
optional boolean check_updates_automatically; a missing field means false.

On startup, enabled copies query the public GitHub latest stable release endpoint
at most once per seven days, including failed attempts. Clock rollback permits a
new attempt. Comparison uses numeric application versions, accepting vX.Y and
vX.Y.Z; commit/build identifiers and prereleases are not update versions. The
cached release is checked against the running and skipped versions each startup.
Remind, dismiss, and Open Github leave it eligible for the next startup, even
before the next weekly request. Skip suppresses only the acknowledged version.
A successful later request replaces cached information. Failure does not discard
a prior reminder. Private/unavailable releases produce a CLI error, not a startup
error dialog or authentication prompt.

The themed popup contains the requested heading, current/available versions,
a read-only brief changelog (up to 2000 UTF-8 bytes), and Open Github, Remind me
later and Skip this one. Installed copies additionally show Update AudioHole.
No action button has a tooltip. Portable copies never offer installer execution.
The existing About/Update identity and owner-approved BLANCO text are unchanged.

## Installer contract and ownership

Publish a stable tag vX.Y[.Z], release notes, and the Release setup executable as
a direct release asset, named AudioHole Installer vX.Y[.Z]-<7-character-commit>.exe.
The displayed version must match the release tag, including its numeric patch.
GitHub's dotted space normalization and the older Install AudioHole vX.Y[.Z].exe
names remain accepted; dirty, Debug, portable and source names are rejected. CI workflow artifacts are not public release
assets and do not trigger notifications. Matching corresponding source must
remain available with releases. The asset must carry GitHub's SHA-256 digest;
without a valid digest/size/trusted repository download URL, browser updating
remains available but automatic installation is disabled.

WinHTTP fetches and BCrypt SHA-256/file work run on a separate cancellable worker.
Neither callbacks nor routing threads perform update work. Requests use normal
Windows TLS verification, no tokens or automatic Windows credentials, bounded
timeouts, at most five HTTPS redirects, and only GitHub API/release asset hosts.
Responses are capped at 512 KiB; installer/cache capacity at 512 MiB. Each download
uses a unique owned directory under update-downloads. Partial, cancelled and
unverified packages are deleted. Only generated GUID download directories are
cleaned; unrelated files are not removed. A successfully launched package remains
until the next check/download can remove it. No extra updater executable is shipped.

The full installer size/hash and executable signature are checked, then checked
again under a read-only file lock that permits execution but denies modification
and deletion. This is integrity verification against GitHub release metadata,
not a code-signing identity claim. The package stays locked through handoff.

The GUI asks about unsaved edits, then uses the existing asynchronous shutdown
path to wait for routing and Plug Board cleanup. After all windows close, the
application releases its instance guard and invokes the verified installer
with the exact registered directory and current-user/all-user scope, then releases
COM on process exit. The installer
waits up to 60 seconds for /UPDATEPARENT=<PID> before normal setup, then retains its
existing running-process guard. UAC cancellation/launch failure reopens AudioHole;
saved data and the reminder remain. Installer update/uninstall ownership and data
preservation boundaries are unchanged. All-user installation uses normal Windows
UAC; ordinary portable operation remains unprivileged.

## Diagnostics and verification

Logging off now still captures Error/Critical events and explicit *-failed events,
including configuration/scanner failures, in the existing bounded diagnostics
history. Scans still print live output. Trace logging remains opt-in; severity and
search filters still apply. Update/state/download failures have their own bounded
32-entry CLI history and participate in search, Errors filtering and Clear log.
The existing 16-session / 512 MiB scan archive limits are unchanged.

Model tests cover version order, weekly policy, cached reminders/skip, release
validation, missing digests, malformed/oversized state, external edits and legacy
preferences. Windows fixture transports verify download size/hash/signature,
read-only sealing, cancellation and cleanup without contacting GitHub. GUI smoke
checks actual preference wiring and all popup actions in both themes without
network or executing an installer. Installer smoke checks the parent-exit handoff
in addition to the existing install/reinstall/uninstall lifecycle.
