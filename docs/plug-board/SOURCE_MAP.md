# Implementation map

All paths below are relative to `Source/modules/plug_board/`. Start with public
contracts, then follow their owning implementation. Significant threading,
resource-lifetime, persistence and failure assumptions are documented in code.

| Area | Entry points / owning files |
| --- | --- |
| Public service / snapshots / backend contract | `include/audiohole/plug_board/module.hpp` |
| Portable schema / typed commands | `include/audiohole/plug_board/library.hpp`, `src/core/library.cpp` |
| Control thread / lifecycle | `src/core/module.cpp`, `module_state.hpp`, `module_state.cpp` |
| Checked intent, local history and recovery | `src/core/module_persistence.cpp`, `storage.cpp` |
| Grouping and source identity | `src/core/observations.cpp`, `include/audiohole/plug_board/observations.hpp` |
| Actual-output placement / compact ages | `src/core/presentation.cpp` |
| Native backend / capability guard | `src/windows/backend.cpp`, `policy.cpp`, `policy_abi.hpp` |
| Visible session events | `src/windows/session_monitor.cpp` |
| Managed process starts / snapshots | `src/windows/live_process_watcher.cpp`, `live_processes.cpp` |
| Native taskbar/source resolution | `src/windows/window_icon.cpp`, `shortcut_icon.cpp`, `icons.cpp` |
| Icon worker / startup deadlines / local retention | `src/windows/live_icons.cpp`, `taskbar_icon_cache.cpp` |
| Reusable view / mailbox / theme | `src/wx/view.cpp`, `view_state.hpp`, `controls.cpp`, `native_theme.cpp` |
| App rows / hover / drag/drop / context menus | `src/wx/socket_view.cpp` |
| Settings / socket ordering / Windows Mixer | `src/wx/settings_drawer.cpp` |
| Socket color/icon rendering and catalogue | `src/wx/socket_style.cpp`, `material_icons.hpp`, `include/audiohole/plug_board/socket_styles.hpp` |
| Standalone-only application frame / tray | `src/wx/main.cpp`, `board_frame.cpp` |
| Core/model/Windows regressions | `tests/` |
| Actual-control GUI smoke | `src/wx/board_smoke.cpp`, `popup_smoke.cpp` |
| Material SVG provenance/license | `resources/socket-icons/` |
| Current schema / integration / validation docs | `docs/` |

Legacy CLI fixture sources under `harness/` are opt-in developer material and are
not linked into or installed with the product. The removed probe/session GUI
sources are absent. The source tree includes the exact AudioHole branding icon;
its bytes match the original host asset. EarTrumpet research/provenance is in
`Source/docs/plug-board/EARTRUMPET_RESEARCH.md`; its implementation was not copied.
