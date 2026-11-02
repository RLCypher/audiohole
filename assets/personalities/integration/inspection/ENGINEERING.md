# Implementation and review standards

These standards apply to all AudioHole implementation, including existing foundation code. Read them with [AGENTS.md](AGENTS.md), [ARCHITECTURE.md](ARCHITECTURE.md), and [DECISIONS.md](DECISIONS.md).

## Readability

Write for a reasonably experienced C++ developer unfamiliar with this project. Use descriptive names, consistent formatting, explicit ownership and failure paths, and small cohesive functions with one responsibility. Prefer straightforward control flow to clever compression. Avoid unexplained abbreviations, magic constants, deep nesting, and unnecessary templates or macros.

## Contracts and comments

Document significant modules, interfaces, classes, and nontrivial functions: purpose, inputs, results, failure conditions, side effects, assumptions, and deliberate limitations. Explain why a decision exists rather than restating each statement. Record substantial rationale near the implementation, with architecture or decision references when useful.

Document resource ownership and lifetime, particularly thread-bound COM initialization and Windows handles. State thread affinity, synchronization assumptions, timing behavior, and bounded-resource invariants. Keep comments accurate when behavior changes. TODOs must identify a specific unfinished action and its context or tracking reference. Do not add redundant line-by-line comments or rewrite correct code merely to increase comment density.

## Audio audit requirements

Before introducing audio processing, identify the execution thread of each operation and the operations allowed there. Explicitly document where allocation, blocking, locks, logging, filesystem work, and GUI calls are prohibited. Describe buffer allocation, ownership, fill/consume/release boundaries, frame counts, sample representation, and channel layout. Explain capture/render timestamps and their interpretation, synchronization decisions, underrun/overrun handling, device invalidation, and recovery. Distinguish measurements, inference, and unverified assumptions. Never claim real-time safety, support, bit-perfect behavior, or validation without evidence.

## Milestone acceptance

Review readability alongside automated correctness:

- Interfaces and non-obvious behavior are understandable without the original author.
- Threading, ownership, lifetime, failure handling, timing, and state transitions are documented.
- Assumptions, deliberate limitations, and evidence are explicit.
- Comments match the current implementation.
- No unnecessary abstraction or compressed control flow obscures the behavior.

Passing tests does not waive these requirements. Review existing work when touching it; prioritize meaningful contract improvements over cosmetic refactoring.
