# Accepted generation checkpoint

The user formally accepted the current avatar library and official name **Horu**. All 796 operating PNGs are frozen by SHA-256 in `checkpoint.json` and the current acceptance ledger. The source commit preserves canonical references, complete generation/intermediate sources, tooling, timing, preview media and prior review findings. This acceptance changes manifest approval metadata and does not regenerate artwork. Existing `default`, `uwu`, and `vanilla` personality IDs and the `UwU/` source directory remain unchanged.

Known baseline limits remain: held key drawings rather than smooth in-betweens; cut transitions for cup placement; regular styles unavailable; no verified main-GUI integration. These are accepted baseline limitations, not permission to redesign the art.

Recover the original generation tree without changing this checkout:

```sh
git archive 3f6dc60 assets/personalities | tar -x -C /path/to/recovery
```

The following acceptance commit contains approved manifests and current ledgers. The source hash inventory remains the authoritative immutable art baseline.
