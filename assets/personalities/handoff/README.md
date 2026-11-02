# Handoff packages

Build after committing the module:

```sh
python assets/personalities/handoff/build_package.py \
  --binary assets/personalities/runtime/build/audiohole_avatar_viewer
```

- `audiohole-avatar-runtime.zip`: approved operating/canonical assets, native
  source/headers, viewer, integration proposal, tests and validation. An optional
  binary is labelled by its actual build platform in `PACKAGE.json`.
- `audiohole-avatar-generation-checkpoint.zip`: full accepted generation tree
  from Git checkpoint `0b41b8e`, including all intermediates and recovery data.
- `packages.json`: source commit, SHA-256 and byte count of both archives.

The builder validates ZIP integrity and all 796 accepted operating PNG hashes in
both archives. Archives/build outputs are local delivery artifacts and are
ignored by Git; this script and the checksum inventory remain reviewable source.
No publishing, push or merge is part of the packaging command.
