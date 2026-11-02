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

## Verified delivery

The archives were built from source commit **`70f23de`**. Both ZIP CRCs, payload
hashes and all 796 operating PNG hashes were checked. Their SHA-256 and byte
counts are recorded in [packages.json](packages.json).

The extracted runtime/source archive configured and built independently, then
passed all 29 native tests and 18 real GUI checks. Its bundled Linux binary also
passed 18 GUI checks launched from `/tmp` without a `--root` argument, finding
the relocated assets beside its own package. See [verification.json](verification.json)
and the retained package configure/build/CTest logs in this directory.

This is an integration-ready source handoff with a Linux validation binary.
Windows build/WIC/DPI/deployment and main-GUI integration remain the primary
development agent's checks, as detailed in the runtime validation report.
