# Profile editor persistence and Apply audit

Baseline: build [#167](https://github.com/RLCypher/audiohole/actions/runs/43691773822),
commit `36a9d75dfb520eac1f713b1c1d7e94bd8f56f4f0` on `engine/audio-audit`,
confirmed by the owner. The engine module from that baseline is unchanged.

## Findings and changes

- Direct GUI Save/reload already preserved both directional
  `exclusive_mode_preferred` flags and the editable tuning parameters. The audit
  did not reproduce a missing serializer field for that checkbox.
- Reproduced a loss before serialization: type a sample rate and an incomplete
  channel map, then change Operating mode. The old panel rebuild replaced both
  fields with the last valid preferences. A subsequent Save could therefore
  write older intent. Inventory, endpoint, engine, mode, tuning-policy and duplex
  refreshes now preserve raw editable text. Section resets and timing presets/
  paste replace only their own section. Loading a different saved record still
  loads that record's values.
- Invalid input now remains visible and blocks Save/Apply. Switching, creating
  or duplicating a hole cannot discard it silently; disabling Return first
  validates its pending edits. The validation message identifies Forward/Return
  and includes latency/buffer settings.
- The owner clarified that all Profile-tab Apply buttons have the same scope.
  The top button and both buttons beside Default device buffer now validate and
  save the **entire profile**, then apply it. The previous local buffer-only GUI
  action has been removed. The lower-level update-buffer API remains unchanged
  for existing callers and is no longer used by these buttons.
- Apply on the active profile saves before requesting asynchronous engine Stop,
  waits for confirmed stream cleanup, then starts the exact accepted profile.
  A later Save during cleanup stays staged and cannot replace that Apply. Stop
  cancels the queued restart. Applying with every hole disabled saves and stops.
  Applying from Stopped retains the existing save-and-start behavior.
  Applying only names/notes to a healthy running profile updates saved/live
  metadata without interrupting its unchanged streams.
- Invalid intent and failed file writes leave existing routing untouched.
  Backend initialization failure after restart retains the saved settings and
  reports the engine failure; automatic rollback to the old route is not claimed.
  Applying a different profile while another is armed remains rejected. Duplicate
  buttons share availability and help, and are disabled during startup/cleanup or
  without a routing backend.

Save remains configuration-only. No profile schema, driver allocation contract,
channel-conversion algorithm, device identity, or user data location changed.
Exclusive preference remains independent of advanced tuning. Active forced
Shared/Exclusive requirements take precedence; a successful shared fallback does
not mean the saved preference was ignored or lost. Clock selection is retained
metadata and still has no implemented clock-policy effect.

## Editable field to JSON mapping

Paths below are relative to `profile` in `profiles/<display name>.json`.
`D` means either `holes[i].forward` or `holes[i].reverse` (Return).

| Control | Canonical JSON field |
|---|---|
| Profile rename / notes | `display_name`, `notes` |
| Hole create, duplicate, delete | `holes` and stable `id` values |
| Hole rename / notes | `holes[i].display_name`, `holes[i].notes` |
| Enabled / Enable return channel | `holes[i].enabled`, `holes[i].duplex` |
| Routing Engine | `holes[i].routing` |
| Capture / playback endpoint | `D.source`, `D.destination`; readable `source_name`, `destination_name` hints |
| Exclusive mode preferred | `D.exclusive_mode_preferred` |
| Use advanced tuning | `D.advanced_tuning` |
| Sample rate / stream channels / sample format | `D.settings.preferences.common.sample_rate_hz`, `channel_count`, `sample_format` |
| Source / destination channels | `D.settings.preferences.source.channel_count`, `destination.channel_count` |
| Channel map | `D.settings.preferences.channel_map` (zero-based indexes; editor is one-based) |
| Routing audio queue | `D.settings.preferences.queue_target_ms` |
| Playback audio queue | `D.settings.preferences.destination.render_ahead_ms` |
| Default / input / output processing interval | `D.settings.preferences.common.period_ms`, `source.period_ms`, `destination.period_ms` |
| Default / input / output device buffer | `D.settings.preferences.common.buffer_ms`, `source.buffer_ms`, `destination.buffer_ms` |
| Operating mode | `D.settings.preferences.common.mode` |
| Placebo clock device | `D.settings.preferences.preferred_clock_device` |

Complete format presets set the format fields above; Manual is an editor choice
for those fields, not a separate engine parameter. Auto/Inherit are hints and save
as null/inheritance. Collapsible-section state, candidate evidence, diagnostics
and panel geometry are presentation state, not profile routing parameters.
Inactive advanced values and the unused Return direction retain their intent.
Unknown JSON extensions retain the existing atomic-write/external-edit protection.

## Verification

- Native-control TestModeSmoke edits every tuning field in both directions,
  profile/hole metadata and policies, invokes top-level Save, then loads the
  actual named JSON using a fresh controller and compares the complete profile.
  A second save clears flags/overrides to false/null, including native checkbox
  clicks. The reproduced mode-refresh loss is an explicit regression assertion;
  invalid Save is refused and correcting the field saves the preserved edit.
- EngineSmoke invokes **all three Apply buttons** with independent Forward/Return
  buffer values, an exclusive preference and profile notes. It checks save before
  cleanup and actual restarted Runtime receipts using software adapters.
- Controller integration tests cover delayed cleanup, a later staged Save,
  Stop cancellation, metadata-only Apply without reopening streams, rejected
  writes, invalid requirements, disabling the last
  hole and initialization refusal with saved intent retained.
- Supplemental cross-build/Wine tests exercise Windows controls and the real
  Runtime with software fixtures. They do not establish native driver timing or
  uninterrupted hardware playback. Native Windows CI and physical-device checks
  remain the platform/hardware validation boundary.

Local results: all **281** portable CTest cases passed; the Windows cross-build
passed; **261** Windows model tests passed under Wine; and Smoke, TestModeSmoke,
TutorialSmoke, EngineSmoke and RestartSmoke all passed (including the restart
child's result). Formatting and whitespace checks passed. The supplemental Wine
model run excludes `ConfigurationBundle.*` and
`PersonalityFiles.MissingOptionalFeatureAndCapabilitiesNeverBorrowFromOtherPackages`
because of the existing MinGW/Wine path-handling limitations; these remain in
native Windows CI. GitHub compilation is not monitored for this change, at the
owner's request.
