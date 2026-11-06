> Current artwork update: [IDLE padding removed from 34 sequences](IDLE_PADDING_REMOVAL.md); 699 operating PNGs remain.

> Main-app integration update: operating art/manifests now live in each owned
> `personalities/<id>/avatar/pack/` folder. This directory holds shared runtime,
> upstream validation and historical handoff. See [current integration](../../AVATAR_INTEGRATION.md).
> The delivery description below records the original PR6 layout.

# Approved avatar artwork, engine and integration handoff

This curated delivery contains the 796 approved operating PNGs for Norm, Horu
and Vanilla, the native C++20 animation engine and standalone wxWidgets viewer,
required manifests/timing/schema data, tests, dependency notices and integration
instructions. Existing `default`, `uwu` and `vanilla` IDs are preserved.

- [Build and launch the engine/viewer](runtime/README.md)
- [Primary-agent handoff and exact application changes](integration/HANDOFF.md)
- [Public API, threading and lifecycle](runtime/docs/API.md)
- [Validation and remaining Windows checks](runtime/validation/REPORT.md)
- [Approved PNG hash ledger](provenance/accepted-assets.json)

Only accepted operating artwork is shipped. Generation intermediates, canonical
reference/source images, review/contact sheets, historical preview media and ZIP
archives are excluded. Full development/recovery history remains available in
[PR #4](https://github.com/RLCypher/audiohole/pull/4). The separate asset-only
alternative is [PR #5](https://github.com/RLCypher/audiohole/pull/5).

The same engine is used by the independent viewer and proposed GUI adapter.
Integration hooks are supplied for review, not applied to the application.
Windows MSVC/WIC/DPI/deployment and actual main-GUI wiring remain unverified.
All changes belong to `assets/personalities/`; no main GUI/audio changes occur.
