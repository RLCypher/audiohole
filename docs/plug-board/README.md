# Plug Board source and integration evidence

The current host contract is [PLUG_BOARD_INTEGRATION.md](../../PLUG_BOARD_INTEGRATION.md).
The source baseline is the existing `work` branch; the handover's host checkout
was not substituted for AudioHole's current engine or GUI.

`INTEGRATION.md`, `PROVENANCE.md`, `ACCEPTANCE_AND_OPEN_ITEMS.md`,
`BUILD_AND_VERIFY.md` and `SOURCE_MAP.md` are archived handover documents. Their
`Source/` paths refer to the supplied ZIP, and statements that host integration
was pending describe that original snapshot. The complete imported module API,
configuration and implementation documents are in
[modules/plug_board/docs](../../modules/plug_board/docs).

`handover-files.json` preserves the archive SHA-256 and original source manifest.
`integration-source-diff.json` identifies every adapted module file and its
original/current hashes. Unlisted imported source files retain their handover
bytes. Host integration files and the final checks are described in
[VALIDATION.md](VALIDATION.md).

The supplied Norm and Horu response documents are retained verbatim here. Their
80 responses and packaged tutorial artwork hashes are checked by
`scripts/CheckPlugBoardInputs.py`. The complete additional Norm tutorial script
is in [docs/tutorial/PlugBoard-Norm-script.txt](../tutorial/PlugBoard-Norm-script.txt).
