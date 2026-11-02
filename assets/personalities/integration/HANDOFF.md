# Avatar engine integration handoff

The generation milestone is formally accepted at **`0b41b8e`**. Artwork and
sources before acceptance are recoverable from **`3f6dc60`**; the acceptance
commit preserves the same pixels with approved manifests and a 796-file hash
ledger. Character names are Norm, **Horu**, and Vanilla. Configuration/personality
IDs remain `default`, `uwu`, `vanilla`.

The native engine and standalone viewer are implemented inside
`assets/personalities/`. **No main AudioHole GUI, audio, model, configuration,
personality, build-system or other out-of-scope files were changed.** Integration
below is proposed for the primary development agent, not already installed.

## Read-only inspection findings

The local avatar branch originally contains only the specification-era
application tree. GitHub's newer **`work`** revision
**`54314f56d08ca89be7b34aaf2a6a808670a83c94`** supplies the actual GUI. Read-only
source copies and Git blob identities are retained in
[inspection/manifest.json](inspection/manifest.json).

| Concern | Verified implementation and implication |
|---|---|
| Language/build | Root CMake uses C++20, CMake 3.25, Windows x64; wxWidgets 3.2.6 and JSON 3.11.3 are existing evaluation dependencies |
| Framework | Native wxWidgets windows/sizers, `wxAutoBufferedPaintDC`, `wxImage` and `wxBitmap`; GUI-owned timers and handlers |
| Placeholder | `src/gui/frame.cpp:96` constructs `AgentDisplay(this, wxSize(144,216))` and sets `placeholder_avatar()` |
| Actual source | Immutable 1201×1310 RGBA Norm reference loaded once through WIC; this is a temporary non-Vanilla placeholder, not the runtime pack format |
| Layout | `layout_header()` computes header height from other columns and calls `fit_height(header_height - 2*FromDIP(6))`; source width is proportional, no crop; avatar follows commentary with a 14-DIP combined gap |
| Compact size | 144×216 is the initial minimum, not a fixed final measured display size. Full header height is dynamic. Viewer also offers representative 128×192 and exact native 512×768 |
| PNG facility | `cmake/WxWidgets.cmake` disables wxPNG/libpng. Existing branding uses Windows WIC + shlwapi; the new adapter follows that path on Windows |
| Personality selection | `settings_view.cpp::select_personality` owns the existing stable string ID and placeholder swap; `Messages` owns authored commentary and repetition history |
| Authoritative data | `Controller::snapshot()` returns borrowed GUI-thread data; copy state/scanning values before asynchronous work. No snapshot vectors/pointers are captured by the asset worker |
| State enum | `Stopped, Ready, Starting, Running, Degraded, Recovering, Failed, Stopping`; no ninth startup/probing state is invented |
| Events | `Event{sequence,type,detail}`, monotonically gated by `seen_event_`; `Frame::refresh()` maps existing events to presentation messages |
| Central copy seam | `Frame::present_message(Message)` calls `Messages::show(..., ++occurrence_)`; this is the proposed fresh-message hook, not a second dialogue generator |
| Current backend | The inspected GUI uses `SimulatedController`; it does not establish real audio-routing behavior or hardware evidence |

The new `Panel::fit_height` reserves the standard 2:3 operating canvas at the
same physical header height. Source margins and the y=704 character ground stay
inside that canvas. The separate brand logo/tray icon remain existing branding
controls. Windows header/DPI/resize smoke assertions must be adapted to the new
512×768 canvas rather than the old 1201×1310 placeholder or square logo.

## Module architecture

`audiohole_avatar` is the reusable C++ playback/asset library.
`audiohole_avatar_wx` supplies platform decoding and a native alpha-rendering
panel. The standalone `audiohole_avatar_viewer` links these same targets. It
does not invoke AudioHole's model, scanner, routing or personality systems.

One owned worker validates/decodes assets. GUI-side polling advances a
monotonic, exposure-driven player and draws cached bitmaps. No image generation,
decoding, file reads, controller calls or personality decisions occur during
painting or on audio callbacks. Weak decoded-frame caching and explicit budgets
bound resource use; see [API/lifecycle](../runtime/docs/API.md).

The proposed [Presenter](presenter.hpp) and
[message/state mapping](proposed_adapter.hpp) compile against the inspected
application's real declarations. They are an integration proposal because no
avatar reaction mapping exists in that implementation. The personality layer
can replace the proposed mapping without changing the engine. The presenter
observes copied state values and existing selected message events; it never
calls a controller command, generates dialogue, mutates identifiers, or runs DFA.

## Proposed authoritative baselines

| Actual condition | Visual baseline | Priority |
|---|---|---:|
| Failed | error static / owned fallback | 100 |
| Degraded | concerned | 70 |
| Recovering | working | 60 |
| Starting | working | 30 |
| Stopping | working | 25 |
| `snapshot.scanning` when no more severe condition exists | working | 25 |
| Stopped / Ready / healthy Running | ambient idle | 0 |

Healthy routing never continuously requests WORKING. Polling and audio buffers
do not produce reactions. Real operational-condition changes invalidate old
pending and active reactions; a completed recovery cannot leave an old panic
playing merely because it had cosmetic priority. Less severe presentation-only
baseline changes can intentionally let a reaction finish; the API then restores
the latest baseline rather than a saved old state.

Suggested transient mappings use existing messages: Disconnected → restrained
Norm angry / Horu panicking / Vanilla wormhole error; ScanCompleted → Norm and
Vanilla happy / Horu celebrating; Created, Activated, Reconnected → happy;
Invalid or ScanFailed → error. Other messages do not automatically animate.
Current severity always blocks stale success/cosmetic feedback.
The snapshot-aware hook also suppresses a retained disconnection after recovery
and an old scan-completion reaction when another scan is already active.

**Missing source notifications:** no probing flag, general apply-in-progress
flag, event severity field, or event timestamp exists in this snapshot interface.
Do not infer these from English detail strings or invent routing states. The
primary agent can add reviewed notifications later. Until source timestamps
exist, reaction TTL starts when a fresh GUI message is observed; it cannot prove
the age of an unobserved historical core event. Existing sequence/occurrence
gates and current baseline severity prevent replay and stale success masking.

## Exact integration work for the primary agent

1. **Root `CMakeLists.txt`:** after existing `add_subdirectory(src/gui)` (and thus after wx and the `audiohole` target exist), include this workspace's [examples.cmake](examples.cmake). It adds the runtime, links the existing GUI target and stages/installs registry, manifests, schemas and the three `Generated/` directories. Reuse parent wx/JSON targets and static-CRT settings. No new GUI framework is chosen; no separate change to `src/gui/CMakeLists.txt` is needed with this placement. Do not ship Python/image-generation tools as runtime dependencies.
2. **`src/gui/frame.h`:** replace only the avatar `AgentDisplay*` field with
   `avatar::Panel*`, add an owned `integration::Presenter` and a dedicated wx
   animation timer. Keep the separate brand logo/tray classes unchanged.
3. **`src/gui/frame.cpp`:** construct the new panel at the existing placeholder
   location; construct the presenter with the deployed asset root; observe the
   current snapshot before selecting the actual personality; poll it from a
   separate 16 ms GUI timer. Do not advance the controller from this timer.
   At the start of existing `refresh()`, observe state/scanning. Stop this timer
   and shut down the presenter before destroying windows.
4. **`src/gui/settings_view.cpp`:** replace the temporary placeholder swap in
   `select_personality()` with presenter selection using the existing selected
   ID (`logo` for Vanilla, `chibi` for Norm/Horu unless a future variant setting
   is approved). At `present_message`, observe the current controller snapshot
   before sending the existing message plus its incremented `occurrence_`.
5. **GUI smoke/package checks:** update source-ratio expectations, test native
   header/DPI/resize behavior, approval/fallback failures, personality switches,
   shutdown, and deployment with real assets. Keep asset failure independent of
   routing. Supply an existing nonmodal diagnostic sink; the proposal logs once
   per changed diagnostic set and has a buffered stderr fallback.

Concrete host snippets are in [frame-hooks.md](frame-hooks.md). They are not
applied patches or a claim of tested AudioHole GUI integration.

## Validation and remaining platform work

[Native/viewer verification](../runtime/validation/REPORT.md) separates executed
Linux checks from remaining Windows checks. The native decoder is compared
against Pillow for every operating RGBA buffer. The viewer is tested in a real
wx/GTK window under Xvfb, including its visible controls and screenshot.

Windows WIC/MSVC compilation, Windows per-monitor-DPI behavior, installation and
actual AudioHole GUI wiring require the primary agent's Windows environment.
The preserved generation limitations remain accepted: held key drawings,
intentional pose cuts for cup placement, and unavailable regular operating art.
No new emotional artwork or speculative desktop-pet window features were added.
