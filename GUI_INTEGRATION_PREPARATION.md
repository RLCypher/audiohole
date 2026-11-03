# GUI refinement and integration preparation — implementation report

Branch: `work`; no merge into the primary branch. First GUI increment was
`88d2e9996d2adc1ee1621eade199d3abf343d613`; implementation commits are
`5138ed2` (persistence/control), `259b959` (packages/metadata), `518a42d`
(GUI modes/restart). The historical schema proposal is a separate documentation
commit; its final SHA is supplied in the delivery message. This report supersedes its earlier pending-work checklist.

1. **GUI:** fixed header/card geometry, frame padding and existing working icon
   retained; dark footer legibility retained. New mode/settings, Bother entry,
   buffer status and capability-driven disabled controls added without replacing
   the view/editor layout. Native appearance/DPI review remains required.
2. **Preview/Test:** original Preview fixtures/behavior remain simulated/session
   only. Test has a real disk-backed configuration controller, starts empty, reads
   and saves isolated provisional JSON across restarts. No production data touched.
   [Persistence contract](PROVISIONAL_TEST_PERSISTENCE.md) documents exact limits
   and production gaps. This is not approved production-schema compatibility.
3. **Settings/restart:** two default-enabled startup checkboxes; next-launch mode,
   dialog, appearance/personality/notifications/startup preview preferences persist.
   Restart uses absolute executable and parent-exit handshake after safe GUI cleanup
   and instance-guard release. Unsaved profile confirmation remains. No route/pending
   operation auto-resumes. Native OS restart behavior is not executed locally.
4. **Buffer:** adjacent Apply remains disabled/desaturated because no backend can
   confirm live values. Editor/saved values are visible separately from explicitly
   unavailable engine-requested/applied values. Existing profile Save/Apply behavior
   remains; Test Save persists without pretending to apply audio settings.
5. **Disabled/hover:** controls use capabilities and disabled native behavior;
   application-painted buttons desaturate. Placeholder dialogs remain interactive.
   Navigation/log generic hover removed; scan level/scope and DFA help explain
   behavior without expanding the DFA acronym.
6. **Beep:** one fixed-width Test Beep/Cancel Beep control follows backend state.
   Cancellation, natural finish, Stop/replacement and overlap rejection are tested.
   Preview is silent simulation; Test rejects playback, no invented audio.
7. **Simulation:** entire Simulate Errors group exists only in Preview, including
   existing directional failure/recovery behavior. No Test heading/gap/actions.
8. **Bother:** Norm and Horu each have30 responses:10 general,10 factual tips,
   5 eggs,5 explicitly fictional facts. Existing Messages weighting/history retained.
   Avatar click or accessible Bother button uses350ms cooldown and warning guard;
   no DFA/controller/configuration side effects. Horu works without borrowed art.
9. **Expression metadata:** all built-in authored responses carry IDs/categories/
   text/preferred/fallback expression. Legacy text/weights/order/Unicode/DFA eggs
   remain intact. [Capability report](AVATAR_CAPABILITY_REPORT.md) comes from
   read-only PR4 inspection at c6d8822259c71c7e1d64cee8fdc996569e29e60d: Norm/Horu20
   approved default-style statics; deadpan static-only. Vanilla12 core, sleeping
   static-only; optional8 unavailable. No asset decoding/hash audit claimed.
10. **Packages:** dynamic folder manifests, stable ID/name distinction, relative
    resources, bounded validation, duplicate/path/link rejection; optional missing
    features disabled, invalid required resources reject whole package. GUI queries
    metadata/events/expressions/resolved avatar paths. Runtime remains shared and
    external. See [package contract](personalities/README.md) and Visitor fixture.
11. **Migration:** built-ins moved to owned folders preserving IDs and dialogue.
    Legacy flat v1/v2 files still load; v1 missing faces means no reaction. A missing
    selection uses a whole available package, never mixed resources. No valid
    package disables personality presentation. Saved desired ID is retained.
    Extract new distributions fresh to avoid old duplicate flat files. Historical
    device/profile migration is proposed only, never executed.
12. **Communication:** ApplicationController decorates the same backend for GUI/
    Tray; typed commands/correlated Accepted/Completed/Rejected/etc., immutable
    snapshots, severity/events, telemetry states, bounded RAII subscriptions and
    non-real-time post queue. See [internal contract](INTERNAL_CONTROL_CONTRACT.md).
13. **Implemented versus proposed:** Test file I/O is real. Routing/scans/beep/
    monitoring remain Preview simulations; no audio backend/scanner/avatar engine,
    CLI/IPC/OBS/Stream Deck/desktop-pet integration is implemented. Live buffer
    updates reject. Measured latency/xruns/applied buffers stay unavailable.
14. **Validation:** portable Debug/Release tests, Windows cross-compilation,
    distribution-tool fixture checks and candidate schema/sample validation are
    recorded in the delivery message. Native Preview/Test smoke checks compile
    and are registered for CI; Linux cannot execute them. Native MSVC/DPI/restart/
    extracted-package acceptance is pending actual Windows execution. Do not call
    compilation proof of hardware/audio/visual correctness.
15. **Before avatar integration:** coordinate owned registry/asset migration with
    PR4 owner; preserve approved manifests/art byte-for-byte, verify decoded assets
    with its loader, link the shared Session once, connect selected stable identity,
    authoritative baselines/severity and transient response occurrences/revisions/
    expiry; clear identity on switch and shut down/join correctly. Session acceptance
    is queued, not playback completion. No duplicated animation implementation.
16. **Review/decisions:** no production schema approval inferred. Historical
    [schema review](docs/schema-review/REVIEW.md) supplies full field mapping,
    missing requirements, candidate versioned schemas/examples, safe migration plan
    and eight owner questions. O-004/O-005/O-006 remain open. These do not block the
    already authorized isolated Test provider or independent GUI milestone.


## Verified local evidence

- Debug portable CTest:93/93 passed; Release portable CTest:93/93 passed.
- Windows x64 MinGW cross-build: complete GUI, Windows harness and test targets
  compiled/linked, with no compiler warnings in the final build logs.
- PowerShell distribution fixtures passed: exact runtime allowlist, metadata,
  overwrite refusal, unexpected/test executable rejection and cleanup; native-only
  execution refused on this Linux host as intended.
- CMake Runtime install inspected: folder manifests/dialogue/owned preview PNGs,
  executable, licenses, reference branding and checklist only; no test executables.
- All70 original pools each in Norm/Horu compare equal to88d2e99 for every text,
  weight and order.60 new Bother responses validated with required category counts.
- Three candidate core JSON Schemas and their examples validate using Draft2020-12;
  all historical/candidate JSON parses. This validates proposal syntax/core shape,
  not product approval or production interoperability.
- Native MSVC, GUI/Test smoke execution, OS restart and visual/DPI acceptance were
  not run locally. CI retains these gates; no unverified Windows package is claimed.
