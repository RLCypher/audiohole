# Source integration into AudioHole

The accepted standalone behavior is ready to hand over. Host integration remains
the next engineering step. The host engine, scanner, root build and GUI were not
modified during module development. Preserve the existing host architecture and
compile this source with the host's toolchain; the delivered executable is a
standalone application, not a DLL/plugin ABI.

## Build boundary

The module exports these CMake targets:

| Target | Responsibility |
| --- | --- |
| `audiohole::plug_board_core` | Observation/grouping contracts and scoped policy experiment contracts |
| `audiohole::plug_board_model` | Portable intent, persistence, service/controller and presentation rules |
| `audiohole::plug_board_windows` | Native preference adapter, session/start events and icon workers |
| `audiohole::plug_board_wx` | Embeddable socket-board content, controls, themes and Settings drawer |

A host-side CMake sketch, to adapt to the host target and source location:

```cmake
# The GUI option enables the reusable wx view as well as the standalone target.
# EXCLUDE_FROM_ALL avoids building the standalone app when only the host needs
# the reusable libraries. Keep the module's full test build as a separate gate.
set(PLUG_BOARD_BUILD_TESTS OFF)
set(PLUG_BOARD_BUILD_DEVELOPER_TOOLS OFF)
set(PLUG_BOARD_BUILD_GUI ON)
add_subdirectory(path/to/modules/plug_board ${CMAKE_BINARY_DIR}/plug-board EXCLUDE_FROM_ALL)
target_link_libraries(AudioHoleHost PRIVATE
    audiohole::plug_board_windows audiohole::plug_board_wx)
```

Supply existing matching `wx::core`, `wx::base` and JSON targets where appropriate.
The view requires matching wx 3.2.6 source through
`FETCHCONTENT_SOURCE_DIR_WXWIDGETS` for its framework generic-dialog source.
Match compiler, architecture, Unicode setup and static runtime choices. No
prebuilt MinGW library from this environment is supplied as a host dependency.

## Application-lifetime wiring

1. Create one `Module` with explicit portable and machine-local roots. Its backend
   factory must return `make_windows_backend(false)`. Leave
   `ModuleOptions::standalone_inventory` false. The factory and backend lifetime
   run on the service control thread, not the GUI/audio thread.
2. Feed the host scanner's playback-only `EndpointInventory` through
   `set_inventory`. Preserve logical device IDs, exact local Windows bindings,
   availability/ambiguity, unavailable saved descriptors and all three role
   defaults. Friendly names are display text, never endpoint identity.
3. Create a parent-owned `desktop::PlugBoardView` using the long-lived module.
   Reuse the host's theme/button/choice factories as needed. Keep choice-wheel
   protection. The host supplies appearance with `set_appearance` and optionally
   handles the Windows Mixer URI through `ViewOptions::open_url`. Set initial
   `icon_pixels` and call `set_icon_pixels(FromDIP(32))` on host DPI changes so
   native icons are resolved at the physical row size.
4. Supply profile descriptors with `set_profiles`. Call `profile_activated` only
   after an actual committed host activation, with a monotonically committed
   generation. Selecting/editing a profile is not activation. Use
   `profile_deleted` for real deletions; partial descriptor lists do not delete
   saved associations.
5. Wire actual tab/window visibility through `set_host_visible`. Hidden,
   minimized or detached views stop full session discovery, pending icon work
   and age timers. Managed application process-start subscriptions intentionally
   remain, so sticky assignments can work while the UI is hidden.
6. Subscribe using lifetime-safe weak mailboxes. Callbacks execute on the service
   thread; post/coalesce GUI events. Immutable snapshots contain owned values,
   not HWND/HICON/COM/process handles. A copied callback can finish after
   unsubscribe, so it must not capture a destroyed widget directly.
7. Detach views before module destruction. Call `begin_shutdown`, keep the host
   event loop responsive, and wait for its future before releasing the module.
   It stops subscriptions, joins workers, flushes state and releases owning COM
   apartments. Its destructor joins as a fallback and does not belong on an
   audio thread. Native Shell/RPC calls have no guaranteed hard timeout.

The concrete API/lifetime sample is in
[the module API](Source/modules/plug_board/docs/API.md). The standalone
`src/wx/main.cpp` and `board_frame.cpp` demonstrate ownership, but their frame,
tray and standalone inventory shim should not be duplicated in the host.

## Behavior to preserve

- Explicit app assignments persist immediately and indefinitely. Their lifetime
  does not depend on Save, activity age, icon eviction or a previous successful
  preference receipt. Reconcile already-running apps and later managed starts.
- Unassigned apps can display in a unique socket matching their fresh actual
  output. This read-only projection must not save an assignment or enroll policy
  ownership. Explicit System assignments also take precedence.
- Commands bind to the selected board and expected revision. Persist desired
  intent before native policy work. Preference acceptance/readback is separate
  from observed playback; do not claim the stream moved from a receipt alone.
- System is first/protected. Socket order, labels, appearance, checkpoint/copy
  behavior, profile associations and unknown JSON members survive host wiring.
- AudioHole/the exact hosting image remain excluded from discovery and live
  routing, including a renamed host. Match complete canonical image/AppID and
  verified current-user process creation evidence, never app labels or PIDs alone.
- Keep the accepted compact Last seen units, two-line rows, playback-name hover,
  taskbar-icon retention, 16 socket colors and 18 symbols. Do not restore the
  removed diagnostic/probe/session navigation or Settings instruction blocks.
- Windows Mixer opens `ms-settings:apps-volume`; opening it does not submit a
  routing preference. Normal user errors remain visible even without Diagnostics.

## Configuration and migration

The standalone root is
`%APPDATA%/AudioHole/AudioHolePlugBoard/plug-board`. `library.json` is portable
schema-v1 intent; `local/` contains machine bindings, history, icon cache and logs.
Choose explicit host roots and a deliberate migration policy before moving data.
Retain the original standalone data/backup files until the transferred library
and exact local bindings have been validated. Never match a migrated endpoint
only by friendly name or treat missing/ambiguous bindings as permission to guess.

Use [CONFIGURATION.md](Source/modules/plug_board/docs/CONFIGURATION.md) and its
schema/example. The checked writer detects cooperating-writer locks and external
edits, retains backups and preserves malformed files. Changing roots/identity or
hand-editing files is separate from ordinary board edits. Local icon cache is
optional presentation data, not routing configuration.

## Integration completion gate

Build/test against the host toolchain; exercise host-supplied inventory, committed
profile activation, tab visibility/theme factories, app drag/context actions and
owned shutdown. Recheck self-exclusion in the real host image. Run the existing
module tests and native GUI matrix, then the host's required checks. Record native
routing/mixer/DPI/hardware outcomes without substituting Wine or simulation for
Windows evidence. No host integration was executed as part of this package.
