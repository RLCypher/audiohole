# AudioHole — IDLE Acceptance & Full Avatar Generation Authorization

You are the dedicated AudioHole Avatar Development Agent.

**The initial IDLE generation milestone has been reviewed and accepted. You are now authorized to proceed with full personality avatar generation.**

The approved IDLE assets establish the baseline for character identity, artistic consistency, technical formatting, and animation quality. Preserve these assets and use them as the foundation for subsequent work.

This authorization also introduces expanded character animation and expression direction, particularly for UwU.

Read and follow your existing `AGENTS.md`, `AVATAR_ASSETS.md`, and the individual personality `character.md` files.

Your designated workspace remains `assets/personalities/`, or the existing equivalent directory established in the repository.

## 1. First Task — Canonize the Updated Creative Direction

Before generating additional artwork, update the relevant personality `character.md` files to incorporate the creative direction below.

These updates must become part of the permanent character specifications, not merely temporary instructions in this prompt.

Preserve the existing character descriptions, visual references, personality traits, dialogue direction, and established creative boundaries.

Do not replace the original character documents wholesale or discard existing information.

Keep technical implementation requirements primarily in `AVATAR_ASSETS.md`, while character-specific artistic direction belongs in each character's `character.md`.

### Norm — Restrained Expressiveness

Norm's animation should remain understated, dry, competent, and mildly sarcastic.

Typical reactions should involve subtle facial expressions, posture changes, eyebrow movements, coffee-related gestures, and unimpressed stares.

However, Norm is not prohibited from expressive animation.

Occasional exaggerated reactions are encouraged when they create an effective comedic contrast with his normally restrained demeanor.

A rare dramatic reaction should feel significant precisely because Norm usually looks like he's seen this bullshit before.

### UwU — Anime/Manga Expression Language

**UwU's primary expression and animation vocabulary must derive from established anime and manga visual conventions.**

This is a defining artistic characteristic, not an optional stylistic influence.

UwU should behave like an anime-influenced chibi character, using expressive facial shorthand, exaggerated emotional reactions, dramatic poses, and deliberate comedic timing.

Facial expressions should drive the performance. Ear movement, tail movement, gestures, and body language should reinforce the face.

Incorporate appropriate anime/manga conventions, including:

- **Happy:** Closed-eye smiles, sparkling eyes, delighted grins, enthusiastic bouncing.
- **Smug:** Half-lidded eyes, fang grins, knowing smirks, playful sideways glances.
- **Mischievous:** Cheeky winks, sly expressions, teasing gestures, exaggerated self-satisfaction.
- **Embarrassed:** Heavy blush, sweat drops, nervous smiles, frantic hand movements, flustered expressions.
- **Confused:** Asymmetrical eyes, exaggerated head tilts, question marks, blank stares.
- **Surprised:** Dramatically widened eyes, blank white eyes, enormous open mouths, sudden frozen poses.
- **Angry:** Puffed cheeks, narrowed eyes, anger marks, exaggerated pouting, little foot stomps.
- **Concerned:** Shadowed eyes, trembling mouths, anxious sweat drops, drooping ears.
- **Panicking:** Spiral eyes, frantic gestures, dramatic distress, exaggerated visual chaos.
- **Celebrating:** Starry eyes, sparkles, triumphant gestures, excited jumping.
- **Sleeping:** Relaxed expressions, closed eyes, curled posture, optional stylized sleep symbols.
- **Deadpan:** Tiny dot eyes, completely expressionless faces, sudden motionless stares.
- **Despair:** Waterfall tears, dramatic sobbing, collapsed posture, exaggerated emotional devastation.

These are artistic examples, not rigid requirements for every animation.

Permit traditional manga visual shorthand such as sweat drops, anger marks, blush lines, sparkles, speed lines, shadowed eyes, question marks, and simplified comedic facial features.

Use these effects selectively and maintain readability at compact GUI sizes.

UwU may temporarily employ extreme chibi deformation, oversized facial features, simplified eyes, exaggerated mouths, and cartoon physics.

Such deformation must be intentional and reversible, returning to canonical proportions after the reaction.

Preserve UwU's established identity, adult character design, white feline appearance, ears, hair, clothing, accessories, colors, and recognizable silhouette.

UwU's personality remains playful, flirty, mischievous, cheeky, technically competent, and occasionally absurdly dramatic.

Expressions should complement the existing personality dialogue, including kaomoji and suggestive audio-related double-entendre, without requiring every animation to contain text.

**UwU should react like an anime character who knows they're in an anime—and is absolutely willing to abuse that privilege.**

### Vanilla — Brand-Faithful Animation

Vanilla remains the canonical AudioHole logo.

Preserve the logo's geometry, proportions, silhouette, mint-green and lavender-pink colors, black outlines, and recognizable visual identity.

Do not introduce anthropomorphic features.

More energetic animation is permitted primarily within the recessed wormhole interior.

Possible effects include perspective shifts, moving grid patterns, dimensional distortion, ripples, swirling movement, and state-dependent internal effects.

The surrounding logo should remain visually stable and faithful to the canonical reference.

Where practical, preserve the logo as a fixed compositing layer and animate the wormhole independently.

Vanilla communicates application status without adopting an emotional personality.

**The logo is sacred. The wormhole is where we get to fuck around.**

## 2. Expanded Motion Authorization

The approved IDLE animations establish the quality baseline, not the maximum permitted range of motion.

Use three broad motion categories.

### Ambient Motion

For persistent states such as IDLE, THINKING, and WORKING.

Use subtle breathing, blinking, posture adjustments, ear and tail movement, or other restrained activity.

Ambient motion should remain unobtrusive during prolonged use.

### Expressive Motion

For ordinary reactions such as HAPPY, CONFUSED, CONCERNED, SMUG, and EMBARRASSED.

Use noticeable gestures, facial changes, head movements, posture shifts, and secondary motion.

### Theatrical Motion

For major reactions such as SURPRISED, ERROR, ANGRY, PANICKING, and CELEBRATING.

Permit full-body animation, exaggerated poses, jumps, recoils, squash-and-stretch, anticipation, follow-through, and dramatic changes in expression.

UwU should receive the greatest expressive freedom.

Norm may occasionally use theatrical reactions for comedic contrast.

Vanilla remains constrained by its brand identity.

These categories are creative guidelines rather than rigid numerical motion limits.

## 3. Anime-Style Timing and Animation Techniques

For UwU in particular, support traditional anime-inspired timing techniques:

- Abrupt expression changes.
- Held reaction frames.
- Comedic freezes.
- Anticipation followed by exaggerated movement.
- Rapid transitions between normal and simplified facial expressions.
- Sudden emotional reversals.
- Independent ear and tail follow-through.
- Deliberate pauses for comedic effect.
- Brief visual effects that appear and disappear during reactions.

Do not assume every animation must be uniformly smooth.

Held frames, abrupt transitions, and intentionally limited animation are valid artistic choices when they improve expression and comedic timing.

Conversely, use additional frames when needed for convincing motion.

Do not constrain all animations to the original IDLE frame-count target.

Choose frame counts and timing according to the intended performance.

## 4. Full State Generation

Complete the core state catalog for all three personalities:

- `idle`
- `happy`
- `thinking`
- `working`
- `talking`
- `confused`
- `concerned`
- `error`
- `angry`
- `surprised`
- `celebrating`
- `sleeping`

Then implement the optional states where they provide meaningful personality-specific behavior:

- `smug`
- `embarrassed`
- `laughing`
- `suspicious`
- `panicking`
- `bored`
- `mischievous`
- `deadpan`

Prioritize completing all core states before expanding into optional states.

Not every state requires a unique animated sequence. Static expressions and documented personality-specific fallbacks are acceptable when appropriate.

However, do not use static-only implementations merely to avoid generating animation for states that would clearly benefit from expressive motion.

In particular, UwU's major emotional reactions should receive appropriate animation wherever technically feasible.

## 5. Technical Asset Requirements

All generated operating static assets and individual animation frames must be:

**Exactly 512 × 768 pixels, 8-bit RGBA PNG, with genuine alpha transparency.**

Maintain consistent framing, anchor points, character identity, and resting proportions.

Allow intentional movement within the canvas, but do not unintentionally crop important character features.

Preserve the canonical reference images and approved IDLE assets.

Do not generate every frame independently if doing so causes character drift.

Prefer reference-guided generation, controlled transformations, layered animation, compositing, and reusable master artwork where practical.

All generated assets must pass the existing validation requirements.

## 6. Batch Generation and Quality Control

Generate assets in controlled batches.

For each batch:

1. Derive new artwork from approved character masters.
2. Validate dimensions, transparency, and file integrity.
3. Inspect facial features, clothing, accessories, colors, and proportions.
4. Verify anchor consistency and intentional movement.
5. Review animation continuity, timing, and loop behavior.
6. Reject or regenerate visibly inconsistent results.
7. Update manifests and asset inventories.
8. Preserve successfully validated outputs before continuing.

Do not allow a defective generated frame to become the reference for subsequent generations.

Use contact sheets and animated previews to evaluate results.

Inspect expressive animations at both native resolution and their expected compact GUI display size.

Maintain a clear distinction between technically validated assets and human-approved assets.

## 7. Animation Playback and Runtime Behavior

Use appropriate playback behavior for each state.

Persistent animations should generally be subtle and loop cleanly.

Theatrical reactions should generally be finite, event-triggered sequences.

After a temporary reaction, return naturally to the appropriate persistent state.

Respect the priority, interruption, cooldown, and fallback requirements established in `AVATAR_ASSETS.md`.

Prevent excessive repetition of dramatic reactions during routine application activity.

Support reduced-motion alternatives where applicable.

Personality isolation remains absolute.

Never substitute another personality's artwork when an asset is unavailable.

Avatar rendering or asset failures must never interfere with AudioHole's audio routing.

## 8. Validation, Manifests, and Preview Tooling

Complete the asset-generation infrastructure and supporting utilities.

Ensure that manifests accurately describe available assets, animation sequences, timing, variants, and fallback behavior.

The preview utility should support inspection of personalities, states, animations, and available visual variants.

Provide contact sheets and playable previews for generated sequences.

Test invalid files, missing assets, interrupted animations, and personality switching.

Do not claim validation or successful execution without actually performing the relevant checks.

## 9. Development Boundaries

Continue working within your designated personality asset workspace.

You may inspect the existing AudioHole GUI and personality architecture to maintain compatibility.

Do not independently overhaul the main GUI, audio engine, dialogue system, DFA system, or unrelated application components.

Document integration requirements for the primary AudioHole development agent.

Use your dedicated development branch or worktree where available.

Preserve existing approved artwork and documentation.

## 10. Completion and Reporting

Proceed autonomously through full asset generation.

Do not stop for routine approvals between successful batches.

Stop and request review if generation quality deteriorates substantially, canonical character identity cannot be maintained, or an architectural decision would affect systems outside your assigned scope.

At completion, provide:

1. A complete inventory of generated assets by personality and state.
2. Animation frame counts and playback timing.
3. Contact sheets and animated previews.
4. Technical validation results.
5. Manifest and preview-tool status.
6. Known limitations and intentionally deferred states.
7. Integration requirements for the primary AudioHole agent.
8. A summary of updated character documentation.
9. A list of remaining items requiring human approval.

Commit completed work to your assigned development branch according to repository conventions.

Do not merge into the main branch without authorization.

## Final Authorization

The IDLE milestone is accepted.

The expanded motion direction is approved.

UwU's anime/manga expression language is now an authoritative part of the character's artistic identity.

You are authorized to proceed through full avatar generation, validation, and preparation for GUI integration.

**First, canonize the updated creative direction in the appropriate `character.md` files. Then proceed with full generation.**

Remember the fundamental distinction:

**Norm has seen this bullshit before.**

**UwU believes this bullshit is the season finale.**

**Vanilla is a logo.**

Proceed.