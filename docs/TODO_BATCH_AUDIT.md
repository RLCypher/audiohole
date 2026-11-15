# Tutorial, layout, and engine timing audit

This increment implements the pending tutorial, presentation, device, layout,
and routing work, including the profile-v4 handover. The final engine contract
retains driver-rounded capacity allocations; it does not impose allocation
identity with the requested milliseconds.

## Behavior reviewed

- Devices have a Rename context action for capture and playback. It uses the
  existing save path, including stable identity, notes, filename validation,
  external-edit checks, and asynchronous scanner-owned completion.
- Tutorial dependency deletion recovers the affected creation section with the
  supplied surprised response. Wrong selections, disabled holes, changed earlier
  assignments, and unsaved edits guide back to the actual missing prerequisite.
  Completion lifts tutorial recovery. Restart Tutorial resets onboarding without
  deleting working profiles. Tutorial delivery remains Norm-only.
- The supplied dialogue revisions and early-profile prefix are in the authored
  source and generated catalog. Norm and Horu each have 15 minor and 15 major
  error responses. Technical detail stays visible; a minor error cannot replace
  an unresolved operational warning.
- App-selected themes reach owned dialogs and the welcome dialog. Enabled Light
  controls use black text, inactive Light tabs are white, and disabled labels
  avoid native embossed drawing. Splitter changes invalidate affected children.
- Reset layout covers seven splitters and collapsed sections without resizing
  the application. Default fields reserve 2 diagnostic lines, 4 profile-note
  lines, and 3 device-note lines. Device cards align with the folder toolbar.
  Scanning has an outer scroll container and equal padded option widths.
- Hidden devices keep their inventory state but do not emit connection notices.
- Armed routing starts complete enabled holes independently, reports the running
  fraction, and handles exact binding loss/return per hole. Other holes retain
  their sessions; Stop cancels waiting. Missing-device waiting is distinct from
  a stream that opened and failed.
- Each direction has three independent tuning sections, scoped resets, timing
  copy/paste, and capability-aware starting presets. Empty Auto/Inherit hints
  never become configuration strings. Examples use the current serializers.

## Engine findings and corrections

1. Native `0x8889000A` identifies Windows endpoint ownership refusal. The failure
   now retains that HRESULT and explains closing conflicting access and using
   Stop/Start. This code alone does not identify an owning application.
2. Capacity requests and actual allocation are separate receipts. Alignment can
   retry an upward allocation, while explicit intervals still require acceptance
   within ordinary one-frame rounding. Diagnostics show the capacity adjustment.
3. Shared playback lead and automatic software queue sizing no longer derive
   directly from allocation capacity. Manual queue targets bypass the automatic
   historical packet guard; effective frame targets remain observable.
4. Explicit playback lead must fit allocation and Exclusive packet requirements.
   Unsupported processing intervals/lead fail visibly. The library adapter
   rejects independent timing controls it cannot confirm instead of claiming
   they were applied.
5. Both native adapters and miniaudio apply scheduling on audio workers. The
   miniaudio wrapper uses the pinned synchronous WASAPI start/stop callbacks,
   including cleanup after failure; its data callbacks remain sample processing
   and atomic counters only. Automatic means Pro Audio MMCSS / Normal relative
   priority. Observations distinguish requested, applied, unavailable, and inactive.
6. Profile storage, transient engine projection, actual stream feedback, and
   scanner capability evidence remain separate. New timing values preserve
   fractions, null inheritance, unknown extensions, and inactive preferences.

The complete timing and persistence contract is in [LATENCY_V4.md](LATENCY_V4.md).
Native Windows driver/hardware validation is still necessary for the reported
headset and advanced-tuning scenario. Cross-build and Wine results cannot prove
acoustic latency, driver compatibility, or the identity of an external owner.

## Validation evidence

- Linux Debug build and complete suite: **265/265 passed**.
- MinGW Release cross-build: passed. Four engine executables passed their
  contract, access, processing, and startup lifecycle checks.
- Supplemental Wine GUI runs: all five modes passed at both **96 and 144 DPI**,
  including restart child completion. Dark/Light and expanded tuning screenshots
  were inspected. Native decoder checks passed **30/30**; foundation checks 4/4.
- Windows model executable: **245/245 passed** when isolating two groups of pre-existing
  cross-environment path cases. The unfiltered run crashes in configuration-bundle
  fixtures that use extended paths. The unchanged scanner executable passes
  75/81, failing extended-path archive cases. A standalone MinGW/Wine probe
  reproduced an empty `lexically_relative` result for `\\?\` paths, while ordinary
  paths work. Another unchanged test compares a compile-time POSIX source path
  with Wine's fully qualified Z: path. These tests remain enabled in CI; no test
  or product code was weakened to hide the supplemental environment limitations.
- GitHub Actions inspection returned Forbidden from this environment. Native
  MSVC CI and Windows package execution must be checked through Actions; these
  supplemental results do not claim they passed.
