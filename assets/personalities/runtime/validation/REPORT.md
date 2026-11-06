> Current padding-removal revision: see [change and fresh validation](../../IDLE_PADDING_REMOVAL.md).
> The earlier curated-package results below are retained as historical evidence.

# Curated runtime validation

The runtime code and all approved PNGs are copied without changes from the
validated full handoff (`c6d8822`). Only documentation was adapted to this smaller
branch. Generation/reference assets and historical previews are not required.

Retained evidence from the full handoff:

- [29 native tests passed](native-tests.json), [CTest log](ctest.log).
- [All 796 native RGBA buffers matched Pillow byte-for-byte](native-pixels.log).
- [29 ASan/UBSan tests passed](asan-ubsan-tests.log), with leak detection disabled
  because LeakSanitizer is unsupported in the managed ptrace environment.
- [18 real wx GUI checks passed under Xvfb](viewer-smoke.json), including all
  personalities, transport, reduced motion, native/compact bounds, operational
  simulations, missing assets and isolation. Historical screenshot is omitted.
- [Original accepted-art/source integrity record](integrity.json).

The Linux host uses GCC 14.2, CMake 3.31.6, wxWidgets 3.2.8 GTK and JSON 3.11.3.
The original serial viewer measurement was 10.99 ms maximum `Panel::present`,
22.5 MiB decoded pixels and one 1,971,120-byte bitmap. These small software-display
checks do not establish target-Windows performance. Decode/validation happens on
a worker; new-frame conversion/scaling still happens on the GUI thread.

The curated tree is independently built and tested before publication. Its
results are recorded in `curated-tests.json`, `curated-ctest.log`,
`curated-pixels.log` and `curated-viewer.json` beside this report. These are distinct
from the retained historical full-handoff evidence above.

Build instructions are in [runtime README](../README.md). Run the supplied
`tests/verify_native_pixels.py` against the new native report for all 796 files;
`tests/gui_smoke.py` exercises a real standalone window using a desktop or Xvfb.

Windows MSVC/WIC, per-monitor DPI, deployment and actual AudioHole GUI/header
integration remain unverified. Follow the [Windows checklist](../docs/WINDOWS_VALIDATION.md).
No audio hardware/routing or real-time performance testing is claimed. Regular
operating art is unavailable; held drawings and deliberate pose cuts remain
part of the approved artwork. No cross-personality fallback is permitted.
