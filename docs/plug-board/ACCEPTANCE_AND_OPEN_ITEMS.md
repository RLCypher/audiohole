# Acceptance and remaining work

The owner accepted the v0.2.6 standalone result with: “Alright, everything looks
good, prepare the handover package.” Earlier feedback reported live routing as
intended. This is owner-reported acceptance of the reviewed application, separate
from an instrumented Windows/hardware/build matrix.

## Verified evidence retained

| Check | Recorded result |
| --- | --- |
| Portable contracts/model/storage/service | 112 tests passed |
| ASan + UBSan, leak detection | Same 112 tests passed |
| Independent source-copy build | Same 112 portable tests passed |
| Windows x64 Release cross-build | Passed, no compiler warnings in final build |
| Windows portable tests under Wine | 111 passed; POSIX-only symlink test excluded |
| Windows icon/backend/evidence fixtures under Wine | 24 passed; one process-metadata test skipped for Wine's aborting stub |
| Current board/status/mixer UI under Wine | 532 assertions passed |
| Argument-free installed/extracted app launch | Actual Exit control and owned shutdown passed under Wine |
| Runtime / source integrity | Release CRCs, manifests, SHA-256 source matches and single-executable layout verified |

Original logs, assertions and screenshots are under `Windows/verification/`.
Handover-specific checks are under `Evidence/`. None of the automated CTest
suites changes live Windows application audio preferences. GUI tests explicitly
use simulation; icon tests use real native fixture windows under Wine.

## Accepted product behavior carried forward

Sticky routing and actual-output placement; app/taskbar icons with retention;
independent socket scrolling; socket reorder/consecutive deletion/context menus;
16 colors and 18 symbols; themes/readable selections; compact Last seen ages;
single-line playback status with full-name hover; clean Settings and its Windows
Mixer button. Probe/Diagnostics/session UI and shipped helper/test executables
remain removed. Internal bounded errors remain available to developer/host code.

## Work remaining after handover

- Resolve the existing first-party license/public-distribution decision before
  public release, as recorded by the host README and [provenance](PROVENANCE.md).
  Third-party notices are included; no project license is selected here.

- Wire the module into the actual AudioHole host, using its inventory, committed
  profile events, theme, visibility and asynchronous shutdown. This has not been
  merged or executed; no root host source was changed.
- Record native Windows build/UBR, actual playback/preference outcomes, mixer
  opening, reboot restoration, endpoint loss/return, WMI/COM teardown and
  Explorer/tray restart across the intended support matrix.
- Run physical DPI/multi-monitor/high-contrast and screen-reader checks. Wine
  font/theme geometry tests are supplemental, not physical DPI proof.
- Measure native visible/hidden idle and churn resource budgets. Bounded queues,
  caches and counters are implemented; no invented CPU/RAM or teardown deadline
  is claimed. Native Shell/RPC calls can block cooperatively owned workers.
- Review the private Windows preference ABI for intended production support.
  Native writes are capability-gated and disabled on unsupported OS/Wine; there
  is no Microsoft compatibility contract or fallback vtable guessing.

The detailed target-PC workflow is in
[Native validation](Source/modules/plug_board/docs/NATIVE_VALIDATION.md).
Uninstrumented items above do not invalidate the owner's standalone acceptance;
they describe the next host-integration and support-validation work.
