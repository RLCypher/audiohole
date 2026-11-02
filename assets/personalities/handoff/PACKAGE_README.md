# AudioHole avatar runtime/source package

This bundle contains the formally approved 796 operating PNGs, their manifests,
canonical references, native C++20 animation engine, standalone wxWidgets viewer,
integration proposal, dependency notices, tests and executed validation evidence.
All operating images are unchanged 512×768, 8-bit RGBA PNGs with true alpha.

Start with [build/launch instructions](runtime/README.md) and the
[primary-agent integration handoff](integration/HANDOFF.md).
The native viewer and proposed AudioHole integration link the **same** engine.
Read-only application source snapshots document the integration seam; they are
not a running copy of AudioHole. No main application files were modified.

From this extracted package directory, build and test with installed dependencies:

```sh
cmake -S runtime -B runtime/build -DCMAKE_BUILD_TYPE=Release
cmake --build runtime/build -j
ctest --test-dir runtime/build --output-on-failure
./runtime/build/audiohole_avatar_viewer --root .
```

The runtime README's longer commands assume the repository layout. In this flat
package, substitute `runtime` for `assets/personalities/runtime` and `.` for the
asset root, including in Windows commands.

If `runtime/bin/audiohole_avatar_viewer` is present, it is the **Linux validation
binary**, requiring matching system wxGTK/C++ libraries. Run it from the package:

```sh
chmod +x runtime/bin/audiohole_avatar_viewer
./runtime/bin/audiohole_avatar_viewer --root .
```

Windows users should build the supplied source with the existing x64
wxWidgets/JSON dependencies as described in `runtime/README.md`; a Linux binary
is not a Windows executable. Windows WIC, DPI, deployment and actual main-GUI
integration remain validation tasks for the primary development environment.

`PACKAGE.json` lists every payload hash and the exact source commit. The accepted
generation checkpoint is `0b41b8e`. The separate
`audiohole-avatar-generation-checkpoint.zip` preserves the **complete generation
workspace**, including intermediate model outputs, masks, settings, generation
scripts, contact sheets, all previews and historical approvals. Keep that archive
for future art corrections; it is not needed for playback.

Horu is the character name. Existing `uwu`, `default`, and `vanilla` personality
IDs and the legacy `UwU/` pack directory are preserved. No future desktop-pet
window features, audio services, dialogue generation or GUI integration are
silently activated by this viewer.
