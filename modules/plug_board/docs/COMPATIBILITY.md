# Compatibility and evidence boundaries

The module targets Windows x64 and C++20. Live per-application writes use the
investigated Windows 11 private AudioPolicyConfig ABI, guarded by native OS,
interface activation, role reads and verified current-user process tokens. It
does not guess alternate vtable slots or invoke a global-clear operation.
Unsupported capability leaves desired configuration intact and exposes native
errors plus an explicit Windows mixer fallback. Windows 10 may support read-only
inspection; private routing writes are disabled there. Wine writes are disabled.

This private interface has no Microsoft compatibility contract. A known interface
activation and accepted readback do not establish all-app behavior, instantaneous
stream migration or reboot persistence. Native build/UBR coverage must be recorded
with the [validation procedure](NATIVE_VALIDATION.md). Administrator privileges,
drivers, services, forced app restarts, registry routing edits and engine hooks
are not required or used.

The user's uploaded inspection archive recorded native Windows build 26300.9974,
45 endpoints and eight application groups/27 sessions. It established read-only
enumeration and exposed the extended-path Shell icon bug corrected in 0.1.2. It
contained no routing, preference restoration, process-start or resource benchmark
results. See [inspection findings](NATIVE_INSPECTION_8852.md). Those observations
cannot validate the newer live workers, routing UI, native DPI or WMI recovery.

Sources and dependencies are original module code plus the existing pinned
wxWidgets 3.2.6, nlohmann/json 3.11.3 and test-only GoogleTest 1.16.0. The unchanged
AudioHole icon remains separate from application icons. The included license
notices apply. EarTrumpet informed architecture/ABI research; its implementation
was not copied. The accompanying research note documents provenance.

Process matching requires verified complete canonical image identity or AUMID,
creation time, same current user and logon session. Inaccessible/protected/shared
owners and generic hosts can remain unresolved. WMI process-start registration
can fail due to service/permission availability; failure remains pending and
visible, without a hidden full-process polling fallback. Cross-process sessions
cannot establish exclusive stream ownership. ASIO/exclusive/application-owned
paths outside observed Windows sessions need separate evidence.

The portable reducer/storage/service tests and simulated GUI suite establish
specific deterministic contracts. ASan/UBSan checks cover the portable model,
not Windows COM/Shell code. Windows executables are cross-built and exercised
under Wine as supplemental evidence. Native audio, real application icons,
provider teardown, Shell stalls, Explorer restart, hardware loss, screen readers,
physical hardware gestures and multi-monitor DPI still need native validation. Native
Shell/RPC calls can block; cooperative shutdown has no invented hard deadline.

Resource limits are explicit: 1,024 queued user commands, 512 callback tasks,
coalesced session snapshots/active hints, 20,000 current monitored sessions,
1,024 copied expired observations, 256 pending icon requests, 512 Shell cache
entries/64 MiB, 256 GUI bitmap entries and a 512-event diagnostic ring. Clean
hidden state waits on events. Dirty metadata coalesces; icon work is visible-row
requested; process discovery is a pending-generation/startup snapshot plus managed start
events. Each matching start refreshes that app's live tokens; accepted assignments
keep the event subscription until shutdown, with no background enumeration loop.
These design limits and operation counters are not measured native CPU/RAM
acceptance. Record visible/hidden idle and sustained churn measurements before
assigning a resource budget or approving host integration.

The owner subsequently confirmed that live routing behaves as intended. This is
user-reported validation, not a recorded reboot/WMI/theme/DPI matrix. Version
0.2.2 adds taskbar/window icon preference, two-line rows, internal-app exclusion,
explicit native scrollbar/popup palette drawing, persistent managed-start watches
and corrected socket drag/delete selection. These new native behaviors still need
target-PC validation. Window icon lookup is request-driven and may fall back for
headless, protected, brokered or pinned-shortcut-only icons. Foreign WM_GETICON
queries have a 50 ms timeout per icon kind; Shell/property calls can still block.

Under Wine, SHGetPropertyStoreForWindow is an aborting stub and is bypassed;
WM_GETICON window pixels, hidden/tool-window exclusion and mismatched creation
tokens are still tested. RelaunchIconResource/AppUserModelID property behavior
requires native Windows evidence. This does not disable that path on Windows.


Version 0.2.3 targets the reported large-versus-small icon mismatch and adds exact
shortcut-icon matching. This does not infer Steam IDs, follow parent PIDs or match
shortcut names. A launcher-only link without a matching AppID, protected window,
ambiguous custom links or an unexposed Explorer-only override can still fall back.
The native fixture deliberately returns different large and small icons; matching
and conflicting .lnk records are exercised separately. The owner's actual Selkie
window/shortcut metadata is not available in this Linux environment.

Confirmation, text/choice/file popups and library validation messages use app
colors; selections paint matched pairs. High-contrast palettes and DPI remain
native validation gates. The 16 optional socket icons are Google Material Symbols
Outlined, Apache-2.0, rather than generated application icons. Original SVGs,
content hashes, source URLs and attribution are included. Existing dependencies
remain wxWidgets 3.2.6, JSON 3.11.3 and test-only GoogleTest 1.16.0.


The owner subsequently confirmed the corrected application icons behave as
expected on their Windows PC, with Selkie's taskbar character icon visible in
the supplied screenshot. This is user-reported evidence, not an instrumented
Windows build/device matrix. Version 0.2.4 only adds PC/Laptop socket artwork and
shortens app hover help; application-icon resolution is unchanged.

The owner then reported a lifetime-dependent regression: a correct taskbar icon
could revert after app closure or depend on launch order. Version 0.2.5 separates
displayed pixels from source invalidation, retains learned taskbar icons in a
disposable machine-local cache, and performs bounded startup refreshes. Native
window fixtures cover discovery before window creation and before a later icon
change, closed-app cache reload/DPI scaling and unrelated-image exclusion. These
run under Wine as supplemental evidence; Selkie and both launch orders still need
confirmation on the owner's PC. Protected/headless apps and windows creating their
icon after the 30-second refresh window may need Refresh or a view reopen.


Version 0.2.6 removes the user-facing diagnostic/probe/session tools and helper
executables from installation. Internal logs remain bounded; source-only developer
fixtures are optional and are not linked into the application. Last seen labels
and per-status device hover text change presentation, not routing/history schemas.
Windows Mixer uses the standard Settings URI; Wine smoke intercepts it, so actual
Windows Settings launch still needs target-PC validation. Font fitting is checked
in all three palettes at normal/larger font sizes; physical DPI/high contrast and
screen readers remain native checks.
