\# AudioHole — Avatar Asset Specification



\*\*Document:\*\* `AVATAR\_ASSETS.md`  

\*\*Status:\*\* Initial specification  

\*\*Scope:\*\* Personality avatar references, asset generation, animation, validation, packaging, and runtime integration



\## 1. Purpose



AudioHole supports personality-specific graphical avatars that provide visual feedback, communicate application status, and generally make themselves useful—or at least entertaining.



This document establishes the common framework for producing, organizing, validating, and displaying avatar assets.



The system must support multiple personalities, different character designs, interchangeable visual styles, static expressions, and animated behaviors without requiring changes to the underlying audio engine.



The avatar system is entirely cosmetic. It must never interfere with audio routing, device management, synchronization, or application stability.



\*\*Core principle:\*\* AudioHole reports what is happening. The personality decides how to react. The avatar displays that reaction.



\## 2. Personality Identity



Every avatar belongs to exactly one personality.



Initial personalities:



| Personality | Character | Visual identity |

|---|---|---|

| Vanilla | AudioHole logo | Minimal, functional, non-character |

| Normal | Norm | Relaxed, sarcastic, understated technician |

| UwU | UwU | Playful, expressive, mischievous technician |



The personality identifier and character name are separate concepts.



Additional personalities and characters must be supported without modifying the core avatar rendering system.



\### 2.1 Personality isolation



Avatar assets must never cross personality boundaries.



The Vanilla logo is not a universal fallback and must never replace Norm, UwU, or another selected personality.



Fallback behavior must remain within the selected personality.



An avatar failure must not change the active personality.



\## 3. Canonical References



Each character may have multiple canonical visual references.



Initially, Norm and UwU have:



\- Regular full-body reference

\- Chibi full-body reference



Both are authoritative representations of the same character.



Regular artwork is preferred for larger presentations, character selection, documentation, and promotional use.



Chibi artwork is preferred for compact GUI avatars and expressive animations.



These are defaults, not hardcoded restrictions. The architecture must allow additional styles and alternative presentation preferences.



\### 3.1 Reference integrity



Canonical reference images are immutable inputs.



The generation pipeline must never overwrite, destructively modify, or silently replace them.



References establish character identity, including:



\- Species or human appearance

\- Facial structure and characteristic expressions

\- Hair, fur, and eye colors

\- Clothing and recognizable accessories

\- Headphones and audio-related equipment

\- Character-specific proportions

\- General color palette and illustration style



Generated expressions may alter facial expressions, gestures, posture, and other naturally movable features.



Temporary props may be added or removed where appropriate, provided the character remains recognizable.



\### 3.2 Character definitions



Each character must have a machine-readable or human-readable definition documenting its visual characteristics and behavioral style.



Codex may draft these definitions by inspecting the references, but they must remain reviewable and editable by humans.



Character definitions must distinguish mandatory identity features from optional accessories and animation-specific props.



\## 4. Asset Organization



Each personality owns its references and generated assets.



Recommended logical structure:



```text

personalities/

&#x20; normal/

&#x20;   references/

&#x20;     regular.png

&#x20;     chibi.png

&#x20;   generated/

&#x20;     regular/

&#x20;     chibi/

&#x20;   character.md

&#x20;   manifest.json



&#x20; uwu/

&#x20;   references/

&#x20;     regular.png

&#x20;     chibi.png

&#x20;   generated/

&#x20;     regular/

&#x20;     chibi/

&#x20;   character.md

&#x20;   manifest.json



&#x20; vanilla/

&#x20;   references/

&#x20;     logo.png

&#x20;   generated/

&#x20;   manifest.json

```



Existing repository conventions and directory structures take precedence where appropriate.



Codex must inspect the repository before creating or relocating directories. Do not reorganize existing user-supplied references unnecessarily.



Generated assets must be clearly separated from their canonical references.



\## 5. Standard Avatar States



All character personalities must support the following logical states.



| State | Intended meaning |

|---|---|

| `idle` | Default neutral behavior |

| `happy` | Positive outcome or favorable interaction |

| `thinking` | Evaluating information or considering options |

| `working` | Active processing, probing, or configuration |

| `talking` | Delivering dialogue or commentary |

| `confused` | Uncertain or unexpected information |

| `concerned` | Recoverable problems or degraded conditions |

| `error` | Failed operation or significant malfunction |

| `angry` | Frustration or personality-specific irritation |

| `surprised` | Unexpected event or result |

| `celebrating` | Successful milestone or noteworthy achievement |

| `sleeping` | Inactivity or stopped operation |



These states describe presentation intent, not audio engine states.



\### 5.1 Extended expressions



The framework must support additional expressions without modifying its core architecture.



Initial extended expressions:



\- `smug`

\- `embarrassed`

\- `laughing`

\- `suspicious`

\- `panicking`

\- `bored`

\- `mischievous`

\- `deadpan`



Extended expressions are optional.



Personality packs may define additional custom expressions and behaviors.



Unknown or unavailable expressions must resolve through documented fallback behavior.



\### 5.2 State availability



`idle` is mandatory for every personality and supported presentation style.



Other states may initially be implemented as static expressions, animated sequences, or explicit fallbacks.



A personality pack must not claim an asset exists when it has not actually been generated.



\## 6. Animation Framework



The avatar framework must support:



1\. Static expressions.

2\. Looping animations.

3\. One-shot reaction animations.

4\. Transition animations.

5\. Personality-specific idle behaviors.

6\. Optional spontaneous animations.

7\. Future layered or composited animations.



Animations must not be tied to a specific image-generation service.



\### 6.1 Initial animation priorities



| Animation | Suggested behavior |

|---|---|

| Idle | Blinking, breathing, subtle motion |

| Talking | Mouth movement and minor gestures |

| Thinking | Looking around, pondering |

| Working | Focused movement or equipment interaction |

| Happy | Brief positive reaction |

| Error | Startle or frustration |

| Angry | Irritated expression or gesture |

| Celebrating | Short enthusiastic reaction |

| Sleeping | Closed eyes, slow breathing |



The framework should support the full state catalog, but the first asset-generation pass need not animate every state.



\### 6.2 Personality-specific choreography



Different personalities must not be forced to use identical animation choreography.



\*\*Norm:\*\* Restrained gestures, slow blinks, eyebrow movements, coffee drinking, understated reactions, and unimpressed stares.



\*\*UwU:\*\* Ear movements, tail swishing, exaggerated expressions, energetic gestures, playful reactions, and theatrical emotional responses.



\*\*Vanilla:\*\* Subtle logo animation or status indication, without requiring character expressions.



These descriptions are creative guidance, not rigid behavior restrictions.



\### 6.3 Animation metadata



Animation definitions should support:



\- Frame sequence or equivalent animation resource

\- Frame timing

\- Looping behavior

\- Playback priority

\- Interruptibility

\- Transition behavior

\- Anchor and positioning information

\- Optional sound or dialogue synchronization metadata



Sound playback is not required for the initial implementation.



\## 7. Technical Asset Requirements



\### 7.1 Operating format



All generated character avatar assets used by the runtime MUST be 512 × 768 pixels (width × height), in portrait orientation, encoded as PNG with an 8-bit-per-channel RGBA color model (32 bits per pixel).



Every static expression and every individual animation frame MUST use this exact format and canvas size. The dimensions and format are requirements, not recommendations. No 256 × 256 runtime character variant is required; the GUI scales the standard assets for display while preserving aspect ratio.



Animation delivery initially uses sequences of individually validated PNG frames. Other internal representations may be supported later, but must not silently change the required operating asset contract.



Canonical reference artwork is exempt from the operating dimensions and MUST remain unmodified at its original resolution.



\### 7.2 Transparency



Generated assets MUST contain a real alpha channel. Unoccupied canvas areas MUST have alpha = 0 (fully transparent); opaque, solid-color, checkerboard, or simulated transparency backgrounds are forbidden.



Antialiased edges and intentional semitransparent effects are permitted. Avoid color fringes and unintended halos around the character. A PNG file extension alone is not proof of valid RGBA data or transparency.



\### 7.3 Framing, scale, and alignment



The 512 × 768 canvas is fixed; the artwork need not fill it. Each character/style must establish a consistent master composition and anchor points, including a stable ground or baseline position where applicable.



All expressions and animation frames for a given character/style MUST preserve the established relative scale, framing, and anchor points. Deliberate motion may temporarily change pose or position, but unintended frame-to-frame jumps, cropping, resizing, or stretching are not acceptable.



Different personalities and visual styles may have different natural silhouettes and occupied regions within the same canvas. Do not distort characters to force identical bounding boxes. Maintain sufficient transparent padding to accommodate planned gestures and motion without clipping.



\### 7.4 Visual quality



Generated assets should preserve the illustration style, proportions, colors, and identity established by the canonical references.



Do not automatically convert characters to low-resolution pixel art. When shown at smaller GUI sizes, scale the operating asset with an appropriate image filter rather than replacing it with an incompatible lower-resolution source.



\### 7.5 Automated validation



Before an asset is marked technically valid, the pipeline MUST verify that:



\- The file is a readable PNG.
\- Its decoded dimensions are exactly 512 × 768 pixels.
\- Its decoded pixel format contains four 8-bit RGBA channels.
\- Its alpha channel is present and contains genuinely transparent background pixels.
\- The asset is associated with the correct personality, style, and state.
\- Animation frames in a sequence all satisfy the same requirements.
\- Framing and anchor metadata are present and internally consistent where required.



Automated checks should flag suspicious opaque borders, clipping, halos, and excessive frame-to-frame displacement for review. Technical validation does not constitute artistic approval. Invalid outputs MUST be reported explicitly and must not be silently resized, flattened, or accepted.



\## 8. Asset Generation Pipeline



Codex is responsible for orchestrating and validating the generation process.



Actual artwork generation may require an external or integrated image-generation capability.



Codex must determine which generation capabilities are available rather than assuming they exist.



\### 8.1 Generation stages



\*\*Stage A — Reference analysis\*\*



Inspect canonical references and character definitions.



Identify defining visual characteristics, available presentation styles, and potential animation constraints.



\*\*Stage B — Master portraits\*\*



Generate standardized neutral portraits for each character and style.



These establish framing, proportions, and the visual baseline for subsequent expressions.



\*\*Stage C — Expression generation\*\*



Generate the required static expressions using approved master portraits and canonical references.



\*\*Stage D — Animation generation\*\*



Generate animation sequences from approved expressions.



\*\*Stage E — Validation\*\*



Check technical requirements, character consistency, and manifest completeness.



\*\*Stage F — Review\*\*



Produce contact sheets and animation previews for human inspection.



\*\*Stage G — Packaging\*\*



Package validated assets for runtime use.



\### 8.2 Generation safeguards



The pipeline must:



\- Preserve original references.

\- Avoid unnecessary regeneration of approved assets.

\- Maintain consistent character identity.

\- Record generation inputs and relevant metadata.

\- Support regeneration of individual states.

\- Detect missing and invalid files.

\- Avoid falsely reporting successful generation.

\- Never treat an unreviewed asset as human-approved.



If image-generation capabilities are unavailable, Codex should prepare the specifications and generation instructions without fabricating finished artwork.



\## 9. Asset Manifests



Each personality must have a manifest describing its available assets.



The manifest should include:



\- Personality identifier

\- Character identifier

\- Available visual styles

\- Default presentation style

\- Available states

\- Asset paths

\- Animation definitions

\- Fallback mappings

\- Technical metadata

\- Validation status

\- Optional generation provenance



Manifests must be human-readable and editable.



JSON is preferred unless existing repository conventions provide a compelling reason otherwise.



Manifest validation must distinguish between missing, invalid, ungenerated, generated, and approved assets.



Do not use ambiguous empty values to represent unknown or unavailable information.



\## 10. Runtime State Management



The audio engine reports operational events.



The personality system interprets those events and selects appropriate reactions.



The avatar renderer displays the resulting assets.



The renderer must not independently reinterpret audio engine conditions into personality behavior.



\### 10.1 Priority and interruption



The framework must support priority-based reactions.



For example, a significant device failure may interrupt a low-priority idle animation.



Routine background processing must not repeatedly interrupt the avatar.



Animations should support configurable duration, cooldown, and interruption policies.



\### 10.2 Event stability



Rapid or repetitive engine events must not produce constant animation changes.



The personality system should support debouncing, cooldowns, and suppression of redundant reactions.



Avatar activity must not introduce unnecessary processing load or interfere with real-time audio operations.



\### 10.3 Spontaneous behaviors



Personalities may perform occasional spontaneous idle animations.



These behaviors must be configurable, nonessential, and capable of being disabled.



They must never interfere with higher-priority application notifications.



\## 11. Fallback Behavior



Fallbacks must remain within the currently selected personality.



Required hierarchy:



1\. Requested animation.

2\. Corresponding static expression.

3\. Selected personality's `idle` asset.

4\. Last valid frame belonging to the selected personality.

5\. Hide the avatar and record a diagnostic.



The last valid frame must not survive a personality switch.



Missing assets must not silently activate Vanilla or another personality.



A missing or invalid mandatory `idle` asset must cause the affected pack or style to fail validation.



An avatar rendering failure must never prevent AudioHole from operating.



\## 12. Preview and Testing



The framework should provide a developer-facing avatar preview capability.



It should support:



\- Personality selection

\- Visual style selection

\- State selection

\- Static expression inspection

\- Animation playback

\- Frame-by-frame inspection

\- Fallback testing

\- Missing asset diagnostics

\- Contact sheet generation



The preview capability should reuse the production asset loader and renderer where practical.



It must not require live audio engine events to test avatar behavior.



\## 13. Performance and Reliability



Avatar rendering must operate independently of time-sensitive audio processing.



Asset loading, image decoding, animation playback, and GUI updates must not execute on real-time audio threads.



The implementation should avoid unnecessary file access, repeated decoding, excessive memory allocation, and unbounded animation workloads.



A failed animation must degrade gracefully.



Avatar activity is always subordinate to AudioHole's primary purpose: reliable audio routing.



\## 14. Compatibility and Extensibility



The framework must support future additions including:



\- New personalities

\- Alternative outfits and visual styles

\- Custom user-supplied character packs

\- Additional expressions

\- Layered facial animation

\- More sophisticated animation engines

\- Seasonal or event-specific artwork

\- Alternate display sizes

\- Optional dialogue synchronization



These are architectural considerations, not requirements to implement every feature immediately.



Existing personality logic, dialogue behavior, DFA functionality, and easter eggs must remain intact unless a separately approved change explicitly modifies them.



\## 15. Initial Implementation Deliverables



The first Codex implementation should produce:



1\. A documented avatar asset schema.

2\. Character definitions derived from the supplied references.

3\. Manifest structure and validation.

4\. Shared state and animation definitions.

5\. A generation pipeline capable of using an available image backend.

6\. Contact sheet generation and asset inspection utilities.

7\. A functional preview interface.

8\. Runtime fallback handling.

9\. Tests covering manifest validation, personality isolation, missing assets, and animation behavior.

10\. Documentation for adding additional personalities.



The initial implementation should prioritize correctness, maintainability, and extensibility over unnecessary complexity.



\## 16. Governing Principles



\*\*Character identity is canonical.\*\* Generated artwork must remain faithful to approved references.



\*\*Personality isolation is absolute.\*\* Assets must never cross personality boundaries.



\*\*IDLE is mandatory.\*\* Every valid personality presentation requires its own fallback.



\*\*Animation is optional.\*\* Static expressions are valid alternatives.



\*\*Generation is reviewable.\*\* Automated output is not automatically approved.



\*\*The engine remains independent.\*\* Avatar behavior cannot compromise audio processing.



\*\*The framework is extensible.\*\* New characters and expressions should not require rewriting core systems.



\*\*The developer retains creative control.\*\* AI may generate artwork, animations, and supporting assets, but the project remains human-directed and subject to review.



\---



\*AudioHole: Because apparently routing audio wasn't complicated enough, we also needed a tiny animated technician to judge you while doing it.\*



## Accepted full-generation extension

The revised IDLE milestone is accepted. Its operating images and individual
frames remain immutable approved baselines; generated reactions remain separate
candidates until human approval. Full core-state generation is authorized,
followed by optional states where they add meaningful personality behavior.

Use ambient loops for persistent behavior, expressive performances for ordinary
reactions, and generally finite theatrical reactions for major events. These
are artistic categories, not numerical displacement limits. Do not constrain
new performances to the original IDLE frame-count target. Static expressions
are also reduced-motion alternatives. A temporary reaction returns to the
current persistent presentation context, with IDLE as the default.

Manifests may include explicit state behavior (category, persistence, static
hold time), animation priority, interruptibility, cooldown, loop, transition
policy and return behavior. Repeated equivalent events must not restart loops
or spam theatrical reactions. Preview and reference runtime remain outside
audio callbacks and do not independently infer engine state. Reduced motion
selects the current personality's static expression, then its own fallback.

Keep all operating images and frames exactly 512 × 768 8-bit RGBA PNG with true
transparent backgrounds. Maintain explicit provenance, validation and approval
status, anchors, file hashes, bounded sequences and personality isolation.


## Accepted generation baseline and native playback assignment — 2026-11-02

The user formally accepted the complete 796-PNG operating library. The UwU character is officially **Horu**, independently of the `uwu` personality identifier and legacy source folder. Runtime engine and standalone-viewer development is authorized within this asset workspace only. Original specifications and references remain preserved; historical naming/examples above are superseded by this identity clarification.
