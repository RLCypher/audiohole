# Endpoint, format and tutorial-exit polish

This batch covers the seven items queued after e64ba67: Light device row focus,
unconditional confirmed tutorial restart, padded three-line Device Notes,
selection-only endpoint dropdowns, mono/stereo candidate presentation, Manual
format selection, and consistent editor/dropdown heights.

## Boundaries

The new format assessment is presentation-only. It retains independent native
formats, checks the selected backend's availability and supported representations,
honors endpoint/common requirements and explicit maps, and rejects conflicting
unsupported evidence. A mono/stereo candidate is not an identical native tuple.
Selecting that explicit preset keeps the source at one channel and destination
at two; merely displaying it changes no preferences. Existing request preparation,
engine code and configuration schemas are unchanged.

The supplied engine-agent contract expects Automatic mono-to-stereo duplication.
The engine module in this checkout still compiles an empty map as matching
indexes with extra outputs silent (`engine/src/channel_mapping.cpp`). Consequently
the new dual-mono **candidate** is compatibility presentation, not confirmation
that this module already duplicates both channels automatically. That behavior
requires the engine agent's corresponding update; this GUI assignment explicitly
prohibits engine modifications. Explicit user maps are never replaced here.

Tutorial restart uses the existing cleanup/parent-handle handshake. Its private
restart argument explicitly selects working mode even if marker persistence
failed. That argument is accepted only with a validated restart-parent handle.
Normal launches retain their ordinary onboarding behavior.

## Validation

- Linux Debug build and complete CTest suite: **270/270 passed**, including five
  new format-assessment regressions.
- MinGW Release cross-build: passed. Windows model executable: **250/250 passed**
  with the same two previously documented Wine path-case groups excluded from
  this supplemental run. CI retains the complete suite without exclusions.
- All five GUI modes passed at **96 and 144 DPI**, including actual restart
  child completion and the working-after-tutorial restart argument. Tutorial
  smoke forces real marker-write failures for both Skip and End with unsaved
  edits; each still requests a working-mode restart. Cancel remains covered.
- Native-control assertions cover selection-only selectors, Manual retention,
  clearing format overrides, editor/dropdown heights, native Device Notes text
  area, layout reset and unchanged persistence/identity behavior. Light focused
  selection and expanded tuning screenshots were inspected at both DPIs.
- Generated tutorial integrity and whitespace checks passed. No engine source
  files were changed.

Cross-build/Wine checks remain supplemental. Native Windows MSVC CI and real
driver/audio-channel validation are separate; no hardware or acoustic result is
claimed here. Package and published CI status are reported with delivery.
