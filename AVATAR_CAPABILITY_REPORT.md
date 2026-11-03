# Avatar capability inspection (read-only PR #4)

Source: PR #4 at `c6d8822259c71c7e1d64cee8fdc996569e29e60d`. Inspected the three manifests, runtime API documentation and native Session header. No source/assets were copied into the application or modified.

Approval status below is declared by those manifests. Every declared operating static/frame path was checked against the source Git tree. This inspection did not decode images, recompute PNG hashes or execute the avatar engine.

| Expression | Norm chibi (`default`) | Horu chibi (`uwu`) | Vanilla logo (`vanilla`) |
|---|---|---|---|
| idle | Approved static + animation | Approved static + animation | Approved static + animation |
| happy | Approved static + animation | Approved static + animation | Approved static + animation |
| thinking | Approved static + animation | Approved static + animation | Approved static + animation |
| working | Approved static + animation | Approved static + animation | Approved static + animation |
| talking | Approved static + animation | Approved static + animation | Approved static + animation |
| confused | Approved static + animation | Approved static + animation | Approved static + animation |
| concerned | Approved static + animation | Approved static + animation | Approved static + animation |
| error | Approved static + animation | Approved static + animation | Approved static + animation |
| angry | Approved static + animation | Approved static + animation | Approved static + animation |
| surprised | Approved static + animation | Approved static + animation | Approved static + animation |
| celebrating | Approved static + animation | Approved static + animation | Approved static + animation |
| sleeping | Approved static + animation | Approved static + animation | Approved static only |
| smug | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| embarrassed | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| laughing | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| suspicious | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| panicking | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| bored | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| mischievous | Approved static + animation | Approved static + animation | Unavailable; owned idle fallback |
| deadpan | Approved static only | Approved static only | Unavailable; owned idle fallback |

Norm and Horu regular styles are disabled and ungenerated. They must not borrow chibi assets. All shipped schema-2 response expression/fallback assignments reference approved statics in the selected character's default style.

## Public integration contract

`audiohole::avatar::Session` takes the selected asset root and decoder. `select(personality_id, style, now)` clears previous identity state. Host-thread `baseline(state, revision, priority, now, invalidate_reaction)` supplies authoritative conditions; `react(Reaction{state, event_id, revision, occurred_ms, ttl_ms, priority}, now)` requests transient reactions. `Decision.accepted` initially means queued, with final admission after `tick()`. The host must not report queued requests as completed playback.

Reaction occurrence IDs must be distinct from stable dialogue response IDs: repeated uses of one authored response are separate occurrences. Preserve monotonic timestamps/context revisions. Operational baseline changes can invalidate a transient response. Healthy active routing maps to relaxed idle. A completed reaction returns to the latest baseline.

GUI/host-thread `tick()` supplies presentations; the loader thread handles decoding with bounded queues. Shutdown cancels work and joins the loader. No session/player/panel calls belong on an audio callback. Fallback remains within one personality: requested animation, matching static, owned idle, owned last valid frame, then hide. Norm/Horu deadpan and Vanilla sleeping use statics.

This is API/asset inspection and data preparation only. No Session, Player or Panel has been linked or instantiated by the GUI.

## Owned package migration preparation

Read-only `registry.json` inspection confirms PR4 maps stable IDs to pack manifests;
Repository resolves `root/registry.json`, then manifest-relative owned artwork.
The proposed per-package `avatar/registry.json` can contain only its own stable ID,
pointing to `pack/manifest.json`. Preserve pack contents byte-for-byte and keep the
engine shared. Metadata resolution is implemented, but no PR4 manifests/art have
been moved or runtime integrated. Missing owned avatar assets report unavailable;
Horu does not display Norm's temporary preview image.


PR6 integration now uses those approved default-style resources through the shared
Session/Panel. Artwork/manifests are unchanged and deployed in owned packages.
See [current integration](AVATAR_INTEGRATION.md); earlier no-playback statements
record the original inspection-only stage.
