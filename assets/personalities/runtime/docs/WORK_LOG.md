# Milestones and evidence

| Milestone | Acceptance criterion | Record |
|---|---|---|
| Accepted generation | Checkpoint approved art, source/intermediate data, manifests/timing and recovery path without pixel edits | `0b41b8e`, `provenance/generation-acceptance/checkpoint.json` |
| Architecture inspection | Verify actual GUI/framework/placeholder/state/message interfaces read-only | Upstream `work` at `54314f56d08ca89be7b34aaf2a6a808670a83c94`; inspection blob inventory |
| Native engine | Same reusable playback/asset code for viewer and proposed GUI, independent of audio/core decisions | `audiohole_avatar`, `audiohole_avatar_wx`; public API and native tests |
| Standalone viewer | All personalities, variants/states, transport, timing, diagnostics, native/compact display and operational simulations | Real wx viewer and GUI smoke JSON/screenshot |
| Handoff | Source/headers/assets/dependencies, integration hooks, test results, recovery and truthful platform limits | Integration handoff, runtime validation report, package inventory/checksums |

Development proceeded within `assets/personalities/`. Build tools/development
headers were extracted into an ignored local `.deps/` directory; no application
files, system packages or primary build files were changed. No push, merge or
pull request was performed. The asset-generation pipeline is frozen against
accidental replacement of approved Horu operating art.
