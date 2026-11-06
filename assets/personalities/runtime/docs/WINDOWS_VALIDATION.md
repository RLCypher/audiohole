# Windows validation required before main-GUI integration acceptance

The executed GUI validation is Linux wx/GTK under Xvfb. It does not establish
Windows WIC, MSVC, deployment or per-monitor-DPI correctness. The code supports
the existing Windows wx 3.2.6 configuration without enabling wxPNG; these checks
remain for the primary agent's Windows environment.

1. Configure/build x64 with CMake/VS 2022 and the existing wx/JSON/static-CRT
   settings. Run CTest and inspect the native test report.
2. Run `tests/verify_native_pixels.py` against the native report: all current deployed WIC
   RGBA buffers should match Pillow. Do not accept a skipped/partial decode.
3. Run `tests/gui_smoke.py` with the built viewer. The Windows screenshot is BMP
   because the parent wx configuration disables PNG encoding; operating art
   remains PNG. Review the actual window, control accessibility and resizing.
4. Exercise 100%, 150%, 200% DPI and moving between monitors. Confirm exact native
   sizing, proportional alpha rendering, no source cropping and stable anchors.
5. Relocate the runtime package; launch with `--root` and automatic root discovery.
   Confirm no dependency on a developer checkout or on AudioHole running.
6. Apply/review the proposed host hooks in the primary application. Check dynamic
   header-height fitting, 14-DIP avatar gap, no old Norm placeholder or cross-pack
   fallback, initialization, reduced motion and all exit/forced-close paths.
7. Drive actual current snapshots and fresh selected message events through the
   adapter. Healthy Running must remain ambient. Failure must beat success,
   recovery must invalidate old panic, and retained historical events must not
   replay after attachment/reload. Avatar failures must not change routing.

The snippets/adapter do not alter the existing personality system or assign
audio responsibilities. Missing probing/apply notifications must be reviewed
as integration work rather than inferred from text or invented states.
