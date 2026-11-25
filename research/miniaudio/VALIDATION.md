# Validation record for the initial published research milestone

- Original assignment was copied byte-for-byte before research (23,719 bytes; SHA-256 in README). The publication policy was later appended; the original byte sequence remains an unchanged prefix.
- Upstream 0.11.25 commit `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d` was independently resolved through GitHub and matched by the local reference checkout. Examined source remains unmodified; snapshot hashes retained.
- Original processing probe compiled with GCC 14.2.0/C99 at O2 and completed all witness assertions. Archived output is software evidence only.
- AddressSanitizer/UndefinedBehaviorSanitizer probe completed with no diagnostics after disabling leak detection. Initial LeakSanitizer attempt failed under the executor's ptrace supervisor; no leak-check success claimed. The guarded s24 witness deliberately has excess backing storage, so it detects a logical output-extent violation without an ASan allocation error.
- Current remote `work` `766ed88f7d7786bfb90fb7a520da34a15de010d1` reviewed before publication; corrected initial-checkout implementation and destination-pacing statements. Current code is a foundation/simulated GUI, not real routing.
- No Windows I/O, hardware latency, CPU benchmark, listening/quality, driver recovery, or long-duration clock tests ran. E2–E11 remain proposed validation.
- Publication allowlist includes research documents, original C/shell experiment code, small textual results/environment metadata, and upstream hash manifest. Excludes downloaded upstream tree, binaries and scratch. All paths are under `research/miniaudio/`.
- No architecture, implementation, other-agent file or accepted decision was edited; no shared checkout switch/reset, force push, or automatic merge.
