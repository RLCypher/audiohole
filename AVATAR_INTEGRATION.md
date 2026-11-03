# Main GUI avatar integration

Source: PR6 at `ab8f1542f4bc3a3fd26940a03e1e7482669cfb4e`.
The initial import is commit `1385b3f`; host integration follows separately. The
upstream handoff describes its older inspected GUI, so its proposed Presenter is
retained as reference, not used as a second response/expression mapper.

## Implemented behavior

The existing header avatar is the shared engine's native `avatar::Panel`, with its
fixed 512×768 (2:3) operating canvas, full-header height, alpha and approved anchors.
Brand logo, executable/taskbar/tray icons and routing controls are unchanged.
Panel checkerboard is off. Changing animation frames never changes layout bounds.

`presentation::AvatarHost` owns one shared-engine Session/worker. It consumes
already selected response metadata and copied authoritative snapshot fields; it
never calls a controller command, selects dialogue, interprets English status
strings or changes settings. `Frame::present` supplies the chosen expression and
one occurrence ID. A repeated authored response has a new occurrence identity.
The same message/occurrence is not generated again by resizing or playback ticks.
By default, GUI reactions retain their final frame for an additional 2000 ms after
one authored pass (or the normal static-image hold), then resume the current baseline. The host
opts into this engine playback policy without editing approved artwork/manifests
or slowing the authored frames. Event freshness TTL remains separate from display
duration. Warning/state changes and personality switches still interrupt at once;
reduced motion keeps its still image for the extended duration. Idle loops and
standalone viewer inspection keep their existing timing.

DFA responses now use individually authored schema-2 expression/subcategory
assignments through the same selection/metadata path as Bother. All 119 existing
Norm/Horu DFA texts, IDs, weights and pool orders are preserved; the selected reply
drives its own pose, including threshold/reference/validation responses. No English
text parsing or separate animation randomness is introduced.

Settings → Personality offers **Hold expressions between events**, unchecked by
default. Checked, it keeps a completed pose until a new reaction or an overriding
baseline change. Another occurrence of the same expression can replay it once the
previous pass has finished (normal event deduplication/cooldown still applies).
Turning the option off releases an already-held pose at its ordinary deadline.
The isolated Preview Build preferences persist the optional boolean
`hold_expressions`; older files omit it and retain timed holds. Failed/conflicting
saves leave both the checkbox and runtime policy at the saved value. The option
does not prevent inactivity sleep or operational interruption.

After two minutes without input, dialogue reactions or meaningful operational
changes, a healthy idle presentation uses its owned `sleeping` state (animated for
Norm/Horu, intentionally static for Status). Healthy routing may continue while
the avatar sleeps. Mouse/keyboard input within the application, window restoration
and new reactions wake it. GUI polling, repainting, resizing and unchanged snapshot
refreshes do not reset inactivity. Scanning, transitions, recovery, failures and
visible operational warnings take precedence and prevent sleep. The host never
changes controller state or commentary to enter/wake from sleep; packages without
sleeping resources retain owned idle. Reduced motion uses the sleeping still image.

The later sleep-text refinement displays `zzz....` over retained awake copy.
An isolated SleepCommentary clock/random schedule may select a short owned `dream`
reply after 45–90 seconds of quiet, display it for six seconds, then return to
snoring. This is the explicit sleep-only exception to event-driven commentary;
ordinary control/playback polling still does not reselect awake responses. Dream
text never calls AvatarHost::react, wakes the avatar, or posts controller events,
logs or notifications. Vanilla/missing pools stay quiet. Warning/wake precedence
removes the rendering overlay without losing the last awake response.

The existing application baseline helper remains authoritative: healthy Running,
Ready and Stopped are idle; transitions/scans use its busy states; Degraded and
Recovering use concern; Failed uses error. An active GUI warning has at least
priority 70. Fresh severe events and real condition/profile changes invalidate old
reactions even if cosmetic priority was high. Routine snapshot revisions do not
restart idle. Stale success/scan-completion reactions are rejected. DFA feedback
shown locally during a warning does not request a competing reaction. Bother keeps
its click cooldown and no visible button/technical line.

The host uses preferred expression when the selected style declares approved
resources, then the response's owned fallback, then owned idle. The engine performs
actual approval/hash/PNG/decoded-pixel checks and animation/static/owned-idle/last-
valid/hide fallbacks. No other character supplies frames. Missing/corrupt assets
cannot activate/stop routing or rewrite configuration. Asset diagnostics are bounded,
deduplicated, nonmodal entries in the All events console; their severity is not
invented by parsing text. Clear log also clears retained avatar diagnostics.

A separate 16 ms GUI timer samples the authored exposure timeline; it does not
advance the controller or turn the artwork into 60 unique drawings/sec. Hidden or
minimized windows stop that timer. Restoring samples current monotonic time. Windows'
client-area animation preference selects reduced motion. Scaling caches one bitmap
per frame/size; painting does no filesystem work, decoding, message selection or
controller calls. The existing 100 ms control timer remains independent.

Personality switches first clear the panel, then cancel/join the old Session and
select the new owned registry/default style. Old identity jobs cannot reappear.
Exit/destruction stop playback before the panel is destroyed; shutdown cancels the
loader and joins it. Decoder work stays on its owned worker (WIC with worker COM on
Windows). All host/Panel/Player calls are GUI-thread only, prohibited on audio
callbacks. No audio engine or hardware routing is integrated by this work.

## Owned package layout and integrity

The 796 approved operating PNGs were moved from the PR6 common directories into:

```
personalities/default/avatar/pack/{manifest.json,Generated/...}
personalities/uwu/avatar/pack/{manifest.json,Generated/...}
personalities/vanilla/avatar/pack/{manifest.json,Generated/...}
```

Each `avatar/registry.json` maps only its personality ID to `pack/manifest.json`.
Artwork and the three approved manifests are byte-preserved (Git attributes also
preserve hashed JSON bytes on Windows); their internal paths,
timing, state/style identifiers, approval flags and anchors are unchanged. No
artwork is generated or edited. Existing temporary preview files remain for
compatibility/reference but are not used as animation fallbacks. A custom package
without valid avatar resources keeps dialogue available with no avatar.

The package loader exposes default style and approved state references, with a
separate 2 MiB bound for avatar manifests (the approved files exceed 64 KiB). Ordinary
personality manifests/registries retain their smaller limits. The shared engine
is compiled once outside packages. No inspection copy of the old GUI is linked
into the app. The optional upstream validation executable uses an isolated build-
tree reconstruction of its original asset layout; no duplicate artwork is stored
in source or installed outside the owning packages.

`provenance/accepted-assets.json` retains the original approval ledger.
`provenance/deployed-assets.json` maps the same hashes to deployed package paths,
plus unchanged approved manifests and owned registries. `scripts/runtime-files.txt`
is the exact frozen distribution allowlist, shared by packaging/validation/tests.
The runtime contains an `avatar-assets.json` ledger. Native ZIP validation compares
assets against trusted source hashes, not an arbitrary self-declared ZIP manifest.
No viewer, tests, Python tools or generation intermediates are installed.

## Small upstream integration patches

- CMake can build core only for portable host tests, reuse parent wx/JSON/CRT,
  select the native test fixture root and bound the validation timeout.
- `wx_panel.hpp` explicitly includes wx/bitmap.h, required by the MSW headers.
- The WIC decoder's NOMINMAX definition is guarded to avoid redefinition.
- Runtime JSON parsing rejects nesting beyond 16, in addition to its byte bound,
  including files externally changed after package discovery.
- Reactions accept a bounded, optional final-frame hold (default zero). The GUI
  requests 2000 ms; immutable clip timing and ordinary engine defaults are preserved.
- The player has an opt-in hold-between-events policy and reports the final hold
  phase, letting the GUI replace that pose with inactivity sleep without cutting
  off an animation that is still playing. Viewer defaults remain unchanged.

No animation/asset-selection algorithm is duplicated or replaced. Upstream source
and validation reports remain available under `assets/personalities/runtime`.
To inspect one owned package in the standalone viewer, pass its `avatar/` folder as
`--root`; the old common root is now a build-tree validation fixture only.

## Verification and platform limits

Portable integration tests execute the actual repository, Session and Player using
real approved PNG input/hash checks and a clearly synthetic RGBA decoder fixture.
They cover all three owned packs, idle progression, selected response expressions,
return to latest baseline, severe-state interruption, stale-success rejection,
identity clearing, missing assets, bounded memory and malformed nesting. They do
not claim native WIC or GUI execution.

The independent Pillow audit decoded all 796 PNGs and matched every straight-alpha
RGBA hash in PR6's native validation reference. The standard-library integrity test
checks the source hashes, dimensions and RGBA header contract. Native CI additionally
runs the upstream validation executable and compares its 796 decoded RGBA buffers
against that reference (`Avatar.NativePixels`). Native GUI smoke checks wait for
owned frames and check the 2:3 geometry, switches, kaomoji wrapping and existing
controller invariants. Extracted-package smoke uses the same deployed assets.

Windows MSVC/WIC execution, monitor-DPI/resize/alpha appearance, reduced motion,
tray hide/restore and restart behavior require native CI/manual evaluation. A
Windows cross-build establishes compilation/linking only. Linux core tests and
Pillow verification do not replace those gates. No visual review or hardware audio
success is claimed here.

Verified locally:103/103 portable tests in Debug and103/103 in Release; Windows
cross-build of the app, native decoder/panel and upstream validation executable;
all796 approved file/pixel hashes (independent Pillow audit); exact820-file Runtime
install with802 manifest/registry/PNG hashes; and PowerShell distribution fixture
checks including unexpected-executable rejection and staging cleanup. Native GUI,
WIC pixel comparison and extracted-ZIP execution are registered CI gates, not
claimed local executions. Branch:work. Kaomoji fix:3c6ef7a; original PR6 import:1385b3f;
owned-package relocation:e024de6; the final host-wiring commit follows those.

## GUI smoke synchronization follow-up

The owner's Windows Release log reports 111/112 tests passing, including the
native avatar engine, WIC pixel comparison and Test Mode GUI smoke. Preview GUI
smoke failed without printing its assertion. Inspection found that its warning
and Bother checks switch packs after the initial avatar wait, clearing the panel;
the later image assertion ran before the GUI-thread poll published the new load.
The smoke test now waits for the selected identity again before that assertion,
checks that waiting never advances controller state, and prints failures and load
diagnostics to stderr as well as `smoke-result.txt`. Production loading remains
asynchronous. The follow-up cross-build and seven focused avatar/layout tests pass
locally; a new native Windows run is still required to confirm GUI smoke passes.

## Reaction settings and inactivity validation

The later reaction/checkbox/sleep/DFA refinements pass 114/114 portable tests in
both Debug and Release, including exact hold deadlines, reduced motion, repeated
same-expression events, warning interruption, two-minute sleep/wake, preference
restart compatibility and per-response DFA metadata. A before/after data audit
confirms all DFA text, weights, IDs and ordering plus every non-DFA entry are
unchanged. Windows cross-builds pass for the GUI, model tests and upstream engine
tests. The native GUI smoke now exercises the checkbox without changing controller
state. Actual Windows execution/visual inspection of these new behaviors remains
pending; portable clocks/decoders do not substitute for that review.
