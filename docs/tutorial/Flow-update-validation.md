# Flow-corrected tutorial update

Baseline: `d49a68792a11c5be7449d2844458d4bc9cbfb4dd` on `work`.

The owner supplied `AudioHole-Tutorial-Agent-Handoff.txt` (SHA-256
`699aa411abcc6e6f33019da26733cd947ab7b3b06640a5719651499ccbd7c514`)
and `AudioHole-Full-Tutorial-Script-Flow-Corrected.txt` (SHA-256
`741c9b6842d9605de47c1063fbfc932e8e83b7f6859e194bc3b67f69186a5031`).
The combined review file was split back into the two editable authoring sources.
Its spoken dialogue is preserved verbatim, including the two runtime-composed
conditional lines. N037/N037A/N037B match the baseline catalogue exactly.

## Resulting behavior

- N032 selects the complete N033 variant. N033's two responses select the
  complete PB001 variant, which waits for actual Plug Board navigation.
  The obsolete N033-to-Settings shortcut is removed.
- The existing socket-creation popup requires a confirmed, nonblank, distinct
  name in Tutorial. Cancel, blank input and duplicate names submit no creation
  command. PB004 advances to PB006 and PB007 to PB009; PB005/PB008 are retired.
  Working-mode naming and the library's command/JSON contracts retain their
  existing defaults. The extra validation is an optional GUI-host policy.
- PB013 allows exploration. Settings navigation rechecks both required socket
  identities, MixBoard outputs and app assignments. A final recheck before N035
  blocks completion if the setup changes after Settings arrival.
- PB-R09 handles either deleted socket, preserving the other socket's valid
  name, output and assignment. The replacement needs confirmed naming, its
  output and the affected app. Recovery retains its specific dialogue and
  guides the user back to the Plug Board tab when necessary.
- PB014, a blank line and N034 form one message, with N034's talking expression
  and two choices. Click overlays restore that exact message without repeating
  the arrival greeting alongside the click reaction.
- The simulated Start Routing operation takes 8,000 ms. Its completion event
  advances the lesson; dialogue length and avatar-click timeouts do not.

## Files changed

| Files | Purpose |
|---|---|
| `personalities/tutorial/tutorial.txt` | Supplied dialogue, cues, choices, expressions and corrected flow |
| `scripts/GenerateTutorial.py`, `src/model/tutorial_catalog.cpp` | Cross-file incoming variants, duplicate-ID rejection and regenerated data: 88 entries in 13 MSVC-safe phases |
| `src/model/tutorial.h`, `src/model/tutorial.cpp` | Choice routing, identity-based creation/recovery, completion gates and Settings composition |
| `src/model/controller.cpp` | One eight-second simulated-activation constant used for timing and progress |
| `modules/plug_board/include/audiohole/plug_board/view.hpp`, `modules/plug_board/src/wx/settings_drawer.cpp` | Optional guided naming guard in the existing themed popup; retain its immutable board across modal message processing |
| `src/gui/plug_board_view.cpp` | Enable the naming guard for Tutorial |
| `src/gui/plug_board_smoke.cpp`, `src/gui/tutorial_view.cpp` | Exercise real popup cancellation/validation, no later rename, revised dialogue, new transition, Settings recovery and combined arrival |
| `tests/tutorial_test.cpp` | Both transition branches, both introductions, naming, either socket's recovery, changed outputs/apps and Settings completion gates |
| `TUTORIAL.md`, `PLUG_BOARD_INTEGRATION.md`, this file | Updated behavior contracts and validation record |
| `docs/plug-board/integration-source-diff.json` | Updated imported-source provenance hashes |

## Verification

| Check | Result |
|---|---|
| Full Linux portable build and CTest | 292/292 passed |
| Focused portable tutorial checks | 25/25 passed, including catalogue integrity |
| Windows x64 MinGW build | Passed |
| Windows tutorial model tests under Wine | 24/24 passed, directly and through CTest |
| Gui.TestModeSmoke / Gui.EngineSmoke under Wine | Both passed through CTest |
| Full Gui.TutorialSmoke under Wine | Passed from the CTest working directory in 43.52 seconds, with fresh PASS evidence |
| Full Gui.Smoke under Wine | Passed from the CTest working directory in 60.18 seconds, with fresh PASS evidence |
| Supplied dialogue / working greetings comparison | Verbatim dialogue preserved; all three working greetings unchanged |
| Catalogue generation | Exact regeneration; 13 named literals below the 8,192-byte limit; retired beats absent |
| Duplicate-definition guards | Separate negative fixtures reject repeated N033 and PB014 definitions |
| Whitespace and affected C++ formatting | Passed |

The initial tutorial GUI run found two stale wording assertions, for the desk
joke and the Return scroll explanation. Both now check the owner's revised
wording. The operation smoke exceeded its 45-second CTest limit under Wine;
the identical checks passed with a supplemental 120-second process watchdog.
Native GUI timeouts and substantive assertions remain enabled.

MSVC Debug/Release, all five native GUI modes, the remaining Windows contracts
and extracted-package launch are evaluated by the Windows Actions workflow for
the published revision. Supplemental Wine evidence does not substitute for
that native verification. The engine, real Windows policy behavior, persistence
schemas, avatar playback and unrelated layouts are outside this change.
