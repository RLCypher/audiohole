# AudioHole Avatar Development Agent

**Scope:** Personality avatar asset generation, animation, packaging, validation, and preview tooling  
**Primary workspace:** `assets/personalities/` (or the existing repository location containing the supplied `Personalities` package)  
**Authority:** `AVATAR_ASSETS.md`, each personality's `character.md`, and canonical reference artwork

## Mission

Develop a repeatable, reviewable pipeline for producing AudioHole personality avatar assets from canonical character references. Deliver validated artwork, animations, manifests, preview tools, and integration documentation without compromising the audio engine or existing personality behaviors.

This is an implementation assignment, not merely a research assignment. Do not claim generated artwork or working integrations that have not actually been produced and verified.

## First Actions

1. Read this `AGENTS.md` in full.
2. Inspect the supplied package and the existing AudioHole repository before changing files. Locate `AVATAR_ASSETS.md`, each `character.md`, canonical reference images, generated directories, existing avatar code, personality logic, and relevant GUI documentation.
3. Preserve the supplied documents and references as the authoritative baseline. Identify inconsistencies, missing requirements, or incompatible existing implementations and report them rather than silently rewriting the specifications.
4. Establish a scoped plan with milestones, dependencies, acceptance criteria, and explicit human-review checkpoints.
5. Determine which image-generation and animation capabilities are actually available. Do not assume Codex itself can generate new illustrations. If no suitable backend is available, prepare prompts, manifests, and tooling; report the blocker rather than fabricating assets.

## Scope and Ownership

You may create and maintain files inside the personality asset workspace, including generated artwork, metadata, schemas, validation scripts, contact sheets, preview utilities, and scoped documentation.

You may inspect the rest of the repository to understand interfaces and constraints. Do not independently rewrite the audio engine, GUI, personality dialogue, DFA system, easter eggs, or shared runtime architecture. Propose integration changes and coordinate them with the primary AudioHole development work.

Use a dedicated branch or worktree where available. Avoid unrelated changes and overlapping ownership with other agents. Do not push or open a pull request without the repository's established workflow or explicit authorization.

## Canonical Inputs

The supplied regular and chibi references are canonical visual references for Norm and UwU. Vanilla uses the canonical AudioHole logo. Each `character.md` defines that personality's appearance, tone, and animation direction.

Do not overwrite, relocate unnecessarily, or regenerate the canonical references. Preserve character identity across all states and frames. Do not invent missing character details where the references and definitions are ambiguous; flag them for review.

`AVATAR_ASSETS.md` governs shared technical requirements and runtime behavior. Character files govern personality-specific creative direction. Existing repository rules remain applicable. Escalate contradictions rather than arbitrarily selecting one source.

## Non-Negotiable Asset Contract

- Operating artwork and individual animation frames must be **exactly 512 × 768 pixels**.
- Operating files must be **PNG with 8-bit RGBA channels** and a genuinely transparent alpha-channel background.
- Maintain consistent character scale, framing, anchor points, and positioning within each personality and style.
- Do not bake checkerboards, solid backgrounds, or unwanted halos into transparency.
- Preserve original reference dimensions; the operating format does not require resizing or replacing references.
- Allow the GUI to scale operating assets for display rather than generating a second mandatory runtime resolution.
- Use the shared state catalog, animation rules, manifest requirements, and fallback hierarchy from `AVATAR_ASSETS.md`.
- Every valid personality/style must provide its own `idle` asset.
- Never substitute assets from another personality. In particular, the Vanilla logo is **not** a universal fallback.
- If an asset fails, fall back within the selected personality as specified; if nothing valid remains, hide the avatar and log the problem without disrupting audio.

Automate checks for dimensions, PNG decoding, RGBA channel presence, alpha transparency, missing files, manifest integrity, and frame consistency. Automated technical validation is not a substitute for visual review.

## Vanilla Brand Protection

Vanilla is the AudioHole logo, not an anthropomorphic character. Its canonical silhouette, proportions, mint-green and lavender-pink colors, black outlines, and general composition must not be substantially changed.

The lower recessed **wormhole** is the designated creative area. Its interior may feature animation, perspective movement, depth effects, or state-dependent variations while the surrounding logo remains recognizable and brand-faithful.

Prefer preserving the canonical logo as a fixed compositing layer and animating the wormhole separately where practical. Never allow iterative image generation to drift the brand colors or redesign the logo.

## Generation Workflow

Work incrementally:

1. **Audit:** Inventory references, definitions, existing code, available generation capabilities, and repository conventions.
2. **Specification:** Establish a machine-readable asset manifest/schema and deterministic naming conventions, consistent with `AVATAR_ASSETS.md`.
3. **Master assets:** Produce one standardized `idle` master per personality and selected style. For Norm and UwU, start with the preferred chibi GUI variant unless repository requirements dictate otherwise. Do not discard regular references.
4. **Review checkpoint:** Produce a labeled contact sheet and a concise report. Await human approval of the master artwork before expanding the batch.
5. **Expressions:** Generate static states from approved masters, checking identity and composition consistency.
6. **Animations:** Generate frame sequences for approved states, with documented timing, looping, transitions, and anchors.
7. **Validation and preview:** Run automated checks and produce an inspectable preview, ideally using the production asset loader where practical.
8. **Handoff:** Document generated assets, gaps, technical findings, integration interfaces, tests, and recommended next steps.

Do not regenerate approved assets without a specific reason. Preserve reproducible generation settings, source references, and provenance when available. Distinguish `not_generated`, `generated`, `validated`, `approved`, `invalid`, and `unavailable` explicitly.

## Animation and Runtime Boundaries

The audio engine emits operational events; the personality system selects reactions; the avatar renderer presents them. Do not embed personality decisions into audio-processing code.

Support static expressions, loops, one-shot reactions, priority, interruption, cooldowns, and personality-specific idle behaviors as required by the specification. Avoid excessive state changes from repetitive engine events.

Never decode assets, run generation, or render animations on a real-time audio thread. Avatar failure must never block startup, device operations, or routing.

Norm's movement is restrained and dry. UwU's is expressive, playful, flirtatious, and animated, including ears and tail. Vanilla is minimal and status-oriented. Follow the individual `character.md` files for detailed characterization; do not duplicate or replace their creative direction here.

## Validation and Acceptance

Before marking a deliverable complete:

- Verify all produced operating images are 512 × 768 RGBA PNGs with meaningful transparency.
- Confirm animation frame dimensions and anchor consistency.
- Check manifests against actual files, states, and fallback paths.
- Inspect contact sheets for character drift, altered accessories, inconsistent colors, stray backgrounds, and malformed anatomy.
- Verify Vanilla's protected colors and geometry remain faithful.
- Test missing assets, invalid manifests, interrupted animations, and personality switches.
- Verify no cross-personality fallback occurs and that a failed avatar cannot affect routing.
- Clearly label generated assets as unapproved until reviewed by a human.

If generation tools cannot produce acceptable character consistency, stop the affected batch, document examples and limitations, and propose alternatives.

## Reporting and Collaboration

Maintain a concise progress record within the scoped workspace. For each milestone, report what was implemented, what was actually tested, what remains unverified, and what requires a decision.

Keep source documents, references, and approved assets intact. Prefer small, reviewable commits with meaningful messages. Do not silently expand scope into the primary application.

The initial handoff should include an asset inventory, schema/manifests, generation capability assessment, validation tooling, first master previews where feasible, and a concrete integration proposal.

**Governing rule:** The avatars exist to make AudioHole more expressive, not to make audio routing less reliable.

*The logo is sacred. The wormhole is where we get to fuck around.*
