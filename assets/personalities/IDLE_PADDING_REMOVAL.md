# Remove IDLE padding from expression sequences

Owner request: remove IDLE frames used as animation padding; the current app does
not require transitions through IDLE when expressions repeat.

The revision removes **97 frames from 34 sequences**: 24 from 12 Norm sequences,
53 from 12 Horu sequences, and 20 from 10 Vanilla sequences. **699 operating PNGs**
remain (52 statics and 647 animation frames across the same 49 sequences).

Only contiguous leading/trailing frames whose decoded RGBA pixels exactly match
an owned IDLE master or IDLE-animation frame are removed. All retained PNG bytes,
anchors, exposure durations and ordering are preserved; numbering restarts at
`0000.png`. Removed time is discarded rather than assigned to the remaining
frames. All IDLE sequences and static expressions remain unchanged.

Finite/loop flags, event holds, cooldowns and GUI controls retain their current
behavior. Existing repeat controls can repeat the shorter expression sequences.
This removes neutral resets; it does not draw new in-between poses or guarantee
that every existing first/last expressive pose forms a perfectly smooth seam.

The original acceptance ledger is historical and unchanged. The current deployed
hash ledger, native RGBA reference and exact distribution allowlist are updated.
The full before/after path/hash/timing mapping is in
[idle-padding-removal.json](provenance/idle-padding-removal.json). The pre-trim
source is recoverable from commit `5bdb28387a686c4a8ea8090db2bde4543d5ab108`.

| Personality | State | Frames before | Frames after | Padding removed (ms) |
|---|---|---:|---:|---:|
| Norm | happy | 12 | 10 | 240 |
| Norm | confused | 14 | 12 | 300 |
| Norm | error | 18 | 16 | 290 |
| Norm | angry | 16 | 14 | 270 |
| Norm | surprised | 14 | 12 | 280 |
| Norm | celebrating | 22 | 20 | 320 |
| Norm | smug | 12 | 10 | 300 |
| Norm | embarrassed | 16 | 14 | 270 |
| Norm | laughing | 18 | 16 | 290 |
| Norm | suspicious | 12 | 10 | 360 |
| Norm | panicking | 20 | 18 | 300 |
| Norm | mischievous | 14 | 12 | 300 |
| Horu | happy | 12 | 8 | 420 |
| Horu | confused | 14 | 9 | 600 |
| Horu | error | 18 | 14 | 470 |
| Horu | angry | 16 | 13 | 360 |
| Horu | surprised | 14 | 11 | 360 |
| Horu | celebrating | 22 | 17 | 590 |
| Horu | smug | 12 | 8 | 500 |
| Horu | embarrassed | 16 | 11 | 540 |
| Horu | laughing | 18 | 12 | 650 |
| Horu | suspicious | 12 | 8 | 580 |
| Horu | panicking | 20 | 15 | 570 |
| Horu | mischievous | 14 | 9 | 600 |
| Vanilla | happy | 12 | 10 | 240 |
| Vanilla | thinking | 16 | 14 | 400 |
| Vanilla | working | 16 | 14 | 300 |
| Vanilla | talking | 12 | 10 | 200 |
| Vanilla | confused | 14 | 12 | 300 |
| Vanilla | concerned | 16 | 14 | 400 |
| Vanilla | error | 18 | 16 | 290 |
| Vanilla | angry | 16 | 14 | 270 |
| Vanilla | surprised | 14 | 12 | 280 |
| Vanilla | celebrating | 22 | 20 | 320 |

## Verification

- Regression tests compare retained hashes and durations against the Git baseline,
  preserve all IDLE/static assets, reject stale files and check every PNG is
  512×768 RGBA with real transparency.
- Re-running the trimming planner finds zero remaining IDLE bookends.
- Native engine and viewer are built from the current app branch using a temporary
  reconstruction of the owned package layout. Fresh reports are saved under
  `runtime/validation/padding-*`; historical upstream reports remain historical.
- No main GUI, audio engine, playback policy or personality/dialogue changes.

Reproduce selection with Pillow installed:

```sh
python assets/personalities/tools/trim_idle_padding.py
python -m unittest discover -s assets/personalities/tools -p test_idle_padding.py
python scripts/branding/verify_avatar_assets.py
```

The regression tests use the preserved baseline commit; a shallow checkout must
fetch that commit before running them. Windows GUI/packaged execution is left to
the existing native CI gates; local Linux viewer checks do not establish it.

Fresh results: **3 regression tests, 29 native tests and 18 real viewer checks passed**.
All **699** native-decoded RGBA buffers match Pillow and the independently remapped
approved pixel reference. The deployed manifest/registry/PNG integrity audit passes.
