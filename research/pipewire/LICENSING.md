# Licensing and source reuse boundaries

## Examined evidence

**Verified from pinned upstream:** `LICENSE` says PipeWire sources are MIT except stated component exceptions: `libspa-alsa.so` includes LGPL PulseAudio code, and `libjackserver.so` is identified as GPL2 due to JACK control headers. `COPYING` supplies the MIT Expat permission and disclaimer. [S18](SOURCE_REFERENCES.md#s18)

| Examined material | Observed license evidence | Reuse implications |
|---|---|---|
| `spa/utils/dll.h`, `ringbuffer.h`, `defs.h` | Per-file MIT SPDX headers and copyright notices | Retain applicable copyright/permission notices for copied or substantial portions; inspect transitive header/build requirements if adapting. |
| Native resampler C/header/SSE files and audioconvert/audioadapter | Examined files carry MIT SPDX headers | Source adaptation requires independent integration review, attribution, validation and explicit approval under the assignment. DSP principles may be independently implemented without bringing in PipeWire. |
| ALSA `alsa-pcm.h`, source/sink node wrappers | Examined headers/wrappers carry MIT SPDX headers; `alsa-pcm.c` itself has no leading license header in this snapshot | Do not infer the entire ALSA plugin is MIT from selected files; the top-level component exception applies to the built plugin. Review provenance before any code reuse. |
| `spa/plugins/alsa/acp/alsa-util.c` | Explicit PulseAudio notice: LGPL version 2.1 or later | Copied/adapted source or distribution of that library needs LGPL-specific review, notice/license/source and relinking obligations as applicable; not recommended as an AudioHole dependency. |
| `spa/plugins/alsa/meson.build` | `spa-alsa` links `acp_lib`, ALSA, math and platform dependencies | Demonstrates that the studied PCM path lives in a larger plugin with more obligations than a standalone controller header. No Linux library is proposed for Windows. |
| JACK server exception | Top-level LICENSE statement only | JACK code/dependencies were not audited; none are recommended. |

The original reference snapshots are an optional local cache under `research/pipewire/upstream/`, with hash verification and copyright headers preserved. Downloaded source/documentation snapshots are excluded from the published research branch; only references and hashes are committed. It is not a production vendor tree, complete upstream source distribution, or dependency acceptance. The harness includes only the original MIT DLL/ring headers; it does not link the ALSA plugin or PulseAudio code. The published branch does not redistribute those downloaded snapshots. Any separately authorized future source redistribution must review all included notices and license-text requirements.

## Distinctions relevant to AudioHole

- **Studying an algorithm:** permits analysis and a decision about principles; no library integration is implied.
- **Independent implementation:** derive a platform-appropriate design and validate it. Avoid assuming mathematical equivalence gives identical tuning, numerical behavior, or artifacts.
- **Adapting source:** separately identify exact files/lines, copyrights/licenses, changes, dependencies, and tests; obtain explicit authorization before production work. MIT is permissive, but it still requires notices; the assignment's approval boundary is independent of license permission.
- **Incorporating a library:** review the exact linked component, dependencies, build configuration, distribution form, version pinning and compatibility with AudioHole's selected license. The baseline has not yet selected AudioHole's release license, so final compatibility cannot be concluded here.

Windows documentation snapshots are official MicrosoftDocs SDK reference pages used as evidence, not a runtime dependency or a recommendation to copy sample code. Each page's source/blob identifier is retained in MANIFEST.json. No proprietary Windows implementation source was examined.
