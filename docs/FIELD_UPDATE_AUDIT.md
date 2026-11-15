# Candidate matching, Update information and field layout

This increment implements the four items queued after c041a6c.

- Matching PCM formats no longer require identical Windows descriptor objects.
  Supported channel-count differences qualify when rate, encoding and precision
  match. Missing native precision metadata uses the canonical sample representation.
  Descriptor-layout differences are
  retained as endpoint facts. Each endpoint has its own candidate label; only
  the mono side receives `2 ch (dual mono)`. Explicit requirements/maps and
  unavailable backends or unsupported formats remain respected.
- Update shares a row with Restart in Application and Tray. Its themed modal
  dialog reports CMake's product version and the executable's build ID, links to
  the AudioHole GitHub repository and closes through its Close action or Escape.
- Unified single-line fields center their native edit client row vertically.
  The Windows edit style is retained, including caret, selection, paste, undo,
  hints and Enter-to-save. Numeric spin buddies use the same centering.
- DFA's editor, stepper and slider share one row, with the slider on the right.

No engine code or persistent configuration format changes are included.
Candidate compatibility is not proof of a verified running hardware route.

## Validation

- Linux Debug build and full CTest suite: **274/274 passed**. Nine format
  assessment tests cover differing descriptors, multiple formats, channel-width
  combinations, partial metadata, explicit maps/requirements and backend limits.
- MinGW Release cross-build: passed. Supplemental Windows model run under Wine:
  **254/254 passed**, excluding the same previously documented extended-path and
  compile-time POSIX-path cases. Windows CI keeps its full suite without exclusions.
- All five GUI modes passed at **96 and 144 DPI**, including real restart-child
  completion. Assertions use actual native text positions, single-line styles,
  padding hit testing, outer field bounds, row alignment and the real Update
  button/dialog/Close path in Dark and Light.
- Update dialog and centered-field screenshots were inspected. Existing tests
  still cover keyboard/input validation, Enter-to-save, notes, unchanged editor
  state, identity, tutorial outlines and operational actions.
- Generated tutorial and whitespace checks passed. Engine source is unchanged.

Wine/cross-build results are supplemental, without hardware/audio verification.
Per the user's instruction, publishing does not wait for GitHub CI to complete.
