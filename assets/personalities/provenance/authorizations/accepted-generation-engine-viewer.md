# AudioHole — Avatar Milestone Acceptance, Animation Engine & Standalone Viewer

You are the dedicated **AudioHole Avatar Development Agent**.

The current personality avatar generation milestone has been reviewed and **formally accepted**.

The generated artwork, expressions, animations, character designs, and associated tooling are approved as the current baseline.

Your next assignment is to:

1. Preserve and checkpoint the approved generation milestone.
2. Canonize the character name **Horu**.
3. Inspect the existing AudioHole GUI and personality architecture under strictly read-only access.
4. Develop a self-contained, integration-ready avatar animation engine.
5. Develop a standalone personality avatar animation viewer using that same engine.
6. Prepare the complete asset and animation-engine package for integration by the primary AudioHole development agent.

The standalone viewer should also establish a foundation for a possible future desktop pet application.

**You are authorized to develop these components within your assigned workspace. You are NOT authorized to modify the main AudioHole application outside that workspace.**

Read and follow your existing `AGENTS.md`, `AVATAR_ASSETS.md`, and the individual personality `character.md` files.

Your designated workspace remains `assets/personalities/`, or the existing equivalent directory established in the repository.

---

## 1. Character Approval and Naming

**Horu** is approved as the official name of the UwU personality's character.

Canonize this name in the appropriate character documentation and asset metadata.

Preserve `UwU` as the existing personality identifier, configuration value, and internal reference unless a future integration decision explicitly authorizes changing it.

The character's name and the personality's internal identifier are separate concepts.

The three personality presentations are:

- **Vanilla:** The canonical AudioHole logo.
- **Normal:** Norm, the human audio technician.
- **UwU:** Horu, the expressive anime-inspired feline character.

Preserve all established visual identities, character descriptions, dialogue behaviors, and animation direction.

Do not rename or restructure existing runtime identifiers merely to accommodate the new character name.

## 2. Preserve the Approved Generation Milestone

Before beginning new development, establish a recoverable checkpoint of the approved artwork and animation library.

Preserve:

- Canonical reference images.
- Approved master artwork.
- Static expressions and animation frames.
- Animation timing and playback metadata.
- Asset manifests and schemas.
- Character definitions and creative documentation.
- Generation scripts and configuration.
- Validation tools and results.
- Intermediate/source assets required for future corrections.
- Contact sheets and animated previews.

Record the current milestone, relevant dependencies, generation settings, and known limitations.

Commit the approved work to the assigned development branch according to repository conventions.

Do not destructively overwrite approved artwork or discard the ability to restore previous results.

Future development may require returning to an earlier expression, animation, or generation method.

**The accepted artwork is a baseline, not an invitation to regenerate everything.**

## 3. Inspect the Existing AudioHole GUI

You are granted **READ-ONLY access to the remainder of the AudioHole repository** for architectural inspection.

Examine the current GUI implementation and determine the best approach for integrating the avatar animation engine.

The GUI already contains a placeholder avatar asset. This is the intended display location for the personality avatars and their animations.

Determine:

- The GUI framework and rendering architecture.
- How the existing avatar placeholder is loaded and displayed.
- Its dimensions, positioning, scaling, and layout constraints.
- The existing personality selection mechanism.
- How personality dialogue and reactions are handled.
- How application state and events are represented.
- Existing event dispatch, notification, and callback mechanisms.
- Available rendering, image-loading, and animation facilities.
- Build-system conventions and relevant dependencies.
- Existing thread-safety and GUI event-loop requirements.

Use the actual implementation as the basis for your design.

Do not assume a particular GUI framework, rendering backend, or event architecture without verifying it.

Prefer reusing existing infrastructure where practical.

## 4. Strict Scope and Modification Restrictions

**DO NOT ALTER ANY COMPONENTS OF AUDIOHOLE OUTSIDE YOUR ASSIGNED SCOPE.**

Your assigned workspace is the personality/avatar development directory.

You may inspect files elsewhere in the repository, but you must not modify, delete, rename, reformat, or reorganize them.

This restriction includes:

- The main GUI and its existing placeholder implementation.
- The audio engine.
- Device discovery and scanning.
- Device probing.
- Routing and synchronization systems.
- Application configuration.
- Personality dialogue and behavior systems.
- DFA functionality.
- Existing easter eggs.
- Shared application infrastructure.
- Unrelated build files and documentation.

If integration requires changes outside your scope, document those changes and prepare proposed patches or code examples **within your assigned workspace**.

Do not apply those changes yourself.

Do not create a competing implementation of existing AudioHole systems.

The primary AudioHole development agent will perform final integration.

## 5. Animation Engine Architecture

Develop a reusable, self-contained animation engine capable of displaying the approved personality artwork within the existing AudioHole GUI.

The engine should support:

- Personality-specific asset loading.
- Asset manifest parsing and validation.
- Static expressions.
- Animated frame sequences.
- Per-frame timing.
- Looping and one-shot playback.
- Personality and state selection.
- Animation priorities.
- Interruptions and transitions.
- Reaction cooldowns.
- Return-to-baseline behavior.
- Personality-specific fallback handling.
- Transparent rendering.
- Scaling to the existing GUI avatar area.
- Reduced-motion alternatives.
- Efficient resource management.
- Graceful initialization and shutdown.

All operating assets remain subject to the established **512 × 768 RGBA PNG** requirement.

The GUI may scale these assets for display.

Do not introduce unnecessary dependencies, external processes, or heavyweight rendering systems.

Prefer a modular design with distinct responsibilities for asset loading, playback, state resolution, and GUI rendering.

## 6. Separation of Responsibilities

**The avatar animation engine is a renderer and playback controller, not a second personality engine.**

Maintain the following separation:

### AudioHole Core

The existing application determines authoritative operational state.

This includes device availability, scanning, probing, configuration changes, routing status, errors, recovery, and other operational conditions.

### Personality System

The existing personality system determines how the selected personality responds to application events.

It remains responsible for dialogue, personality-specific behavior, reaction selection where already implemented, and DFA functionality.

### Avatar Animation Engine

The new module translates the requested personality/state presentation into the appropriate visual asset or animation.

It manages playback, timing, transitions, interruption, fallback, and rendering.

The avatar engine must not independently generate dialogue, make unrelated personality decisions, activate DFA, or duplicate application-state logic.

**Personality affects presentation, not operational truth.**

## 7. Application State Awareness

The avatar system must display animations appropriate to AudioHole's actual current operational condition.

Inspect the existing application-state and event infrastructure to determine how authoritative information can be delivered to the animation module.

Relevant conditions may include:

- Application startup and initialization.
- Ready or idle status.
- Device discovery and scanning.
- Device probing and compatibility testing.
- Configuration loading and application.
- Starting and stopping audio routing.
- Healthy active routing.
- Successful connections and configuration changes.
- Device disconnection.
- Recoverable warnings.
- Operational errors.
- Critical failures.
- Recovery procedures.
- Personality switching.
- Application shutdown.

These are illustrative categories.

Do not invent application states or assume that corresponding events already exist.

Use existing state and event definitions wherever possible.

Where required notifications or mappings are absent, document the necessary integration work for the primary development agent.

## 8. Persistent Operational States vs. Transient Reactions

Distinguish between persistent operational conditions and temporary events.

**Persistent conditions** establish the baseline avatar presentation.

**Transient events** may trigger finite reactions before returning to the appropriate baseline.

For example, a successful connection may trigger a brief HAPPY or CELEBRATING reaction.

After that reaction completes, the avatar should return to the presentation corresponding to the application's **current authoritative operational state**.

Do not automatically return to IDLE if another persistent state is more appropriate.

If the underlying operational state changes during an animation, reevaluate the appropriate presentation.

Higher-priority conditions must be able to interrupt lower-priority reactions.

Do not allow stale, delayed, or queued reactions to misrepresent the application's current condition.

## 9. Healthy Routing Is Normal Operation

**Active, healthy audio routing is AudioHole's normal operating condition. It must not be treated as continuous WORKING activity.**

When audio routing is functioning normally, the avatar should generally display a relaxed, personality-appropriate ambient animation.

Examples:

**Norm**
- Relaxed posture.
- Occasional coffee sipping.
- Subtle blinking.
- Understated idle behavior.

**Horu**
- Casual tail swaying.
- Ear twitching.
- Playful idle expressions.
- Subtle breathing and relaxed movement.

**Vanilla**
- Gentle wormhole movement.
- Minimal, restrained animation.
- Stable canonical logo appearance.

The WORKING state should primarily represent active operations requiring attention or processing, such as device scanning, probing, compatibility testing, applying configuration changes, establishing connections, and recovery procedures.

Do not repeatedly trigger WORKING simply because the audio engine is processing audio buffers.

Routine polling, normal audio processing, and healthy routing activity should not produce repetitive reactions.

The avatar should not appear stressed, unusually busy, or excessively animated merely because AudioHole is functioning correctly.

## 10. Animation Selection, Priority, and Correctness

Animation selection must account for operational severity, personality behavior, event priority, and current application state.

Critical failures and active errors should take precedence over routine successes, cosmetic events, and ambient animations.

Implement appropriate:

- Event deduplication.
- Reaction priorities.
- Interruptibility.
- Cooldowns.
- State reevaluation.
- Transition handling.
- Stale-event rejection.
- Fallback resolution.

Avoid animation thrashing caused by rapidly changing or repetitive notifications.

The same event may produce different visual reactions for each personality.

For example, a device disconnection might cause:

- **Norm:** An irritated glance or understated facepalm.
- **Horu:** A dramatic anime-style panic reaction.
- **Vanilla:** A restrained disruption within the wormhole.

These reactions must represent the same underlying condition.

The engine should receive personality-specific reaction requests from the existing personality system wherever that functionality already exists.

If no such mapping exists, provide a proposed mapping layer and integration contract rather than independently rewriting personality logic.

## 11. Personality Isolation and Asset Fallbacks

Personality isolation is absolute.

Never substitute another personality's assets when an animation is missing or invalid.

In particular, Vanilla is not a universal fallback for Norm or Horu.

Follow the personality-specific fallback hierarchy established in `AVATAR_ASSETS.md`.

If no valid presentation remains, hide the avatar and log the failure without disrupting the application.

Personality switching must invalidate or replace inappropriate queued reactions and prevent frames from the previous personality from appearing.

Preserve approved character identity and animation direction.

Horu's anime/manga expression vocabulary, Norm's restrained reactions, and Vanilla's protected brand identity remain authoritative.

## 12. Performance and Reliability

The animation system is nonessential to AudioHole's core function.

**Avatar rendering must never interfere with audio routing.**

Do not perform asset generation, expensive image decoding, or blocking file operations on real-time audio threads.

Avoid blocking the GUI event loop with expensive processing.

Use efficient loading, caching, and memory-management strategies.

Support clean shutdown and resource release.

Handle missing files, corrupt frames, invalid manifests, and unexpected state transitions gracefully.

Use the GUI's existing rendering facilities and scheduling mechanisms where practical.

Do not introduce unnecessary background services or external hardware requirements.

## 13. Integration Interface

Expose a clean, documented interface suitable for use by the primary AudioHole development agent.

The interface should support, as appropriate:

- Selecting a personality.
- Selecting a visual variant.
- Updating the current operational baseline.
- Requesting a transient reaction.
- Providing event severity or priority.
- Starting and stopping playback.
- Enabling reduced-motion behavior.
- Querying playback status.
- Handling missing or invalid assets.
- Initializing and shutting down the module.

Prefer existing application types and interfaces where practical.

Do not expose unnecessary internal animation implementation details to the rest of the application.

Document threading assumptions, ownership, lifecycle requirements, and error handling.

The integration should require minimal changes to existing AudioHole components.

## 14. Integration-Ready Deliverables

Prepare a complete handoff package within your assigned workspace.

Include:

1. Animation engine source code.
2. Public interfaces and required headers.
3. Approved avatar assets and manifests.
4. Asset-loading and validation support.
5. Build dependencies and configuration requirements.
6. Personality/state integration contract.
7. Playback and lifecycle documentation.
8. GUI rendering integration instructions.
9. Proposed integration patches or code examples.
10. Tests and validation utilities.
11. A precise inventory of changes required in the main AudioHole application.

Do not apply proposed integration patches outside your workspace.

The module must be prepared for integration, not represented as already integrated.

## 15. Standalone Avatar Animation Viewer & Future Desktop Pet Support

Develop a standalone, functional **AudioHole Personality Avatar Viewer** as a required deliverable.

The viewer must operate independently of the main AudioHole application.

It serves three purposes:

1. Inspect, test, and demonstrate the personality animation library.
2. Exercise the same animation engine intended for the AudioHole GUI.
3. Establish an architectural foundation for a possible future desktop pet application.

### Standalone Viewer Requirements

Provide:

- Personality selection between Vanilla, Norm, and Horu (UwU).
- Visual variant selection, including chibi and regular where available.
- Selection of all available animation states.
- Static expression and animated sequence playback.
- Play, pause, restart, and frame-step controls.
- Adjustable playback speed.
- Optional display of animation metadata, frame counts, and timing.
- A checkerboard or other transparency-friendly preview background.
- Preview at native resolution and expected GUI display size.
- Controls to simulate operational states and transient application events.
- Inspection of priorities, interruptions, cooldowns, and fallback behavior.
- Visible diagnostics for missing or invalid assets.

The viewer must use the **actual production animation engine and manifests**.

Do not create a separate mock animation implementation.

### Future Desktop Pet Architecture

Design the viewer and animation engine with potential future desktop pet functionality in mind.

Possible future features include:

- Borderless, transparent desktop window.
- Always-on-top mode.
- Click-and-drag positioning.
- Remembered window position.
- Optional click-through behavior.
- Independent scaling.
- Multiple monitor support.
- System tray integration.
- Context menus.
- Personality-specific autonomous idle behaviors.
- Optional live connection to AudioHole's operational state.

These are future expansion targets, **not mandatory initial viewer features**.

Do not allow speculative desktop pet development to delay the core animation-engine assignment.

Avoid architectural decisions that would unnecessarily prevent these features from being added later.

Keep animation playback independent of window-management behavior.

### Component Separation

Maintain a clear distinction between:

1. **Animation Engine:** Asset loading, playback, timing, transitions, and state resolution.
2. **Viewer Application:** Standalone controls, preview rendering, diagnostics, and interaction.
3. **Future Desktop Pet Host:** Desktop window behavior, positioning, transparency, and optional application connectivity.
4. **AudioHole GUI Integration:** Embedding the same animation engine into the existing application.

Do not duplicate the animation engine for different hosts.

Where practical, reuse the GUI framework and rendering technology already employed by AudioHole.

### Independent Operation

The viewer must function without AudioHole's audio engine, scanner, or routing infrastructure running.

When operating independently, it should allow manual selection and simulation of states and reactions.

If future live connectivity is considered, define a clean optional interface rather than introducing a hard dependency on the main application.

Do not modify AudioHole to support the viewer.

### Viewer Deliverables

Provide:

- Standalone viewer source code.
- Build and launch instructions.
- Required dependencies.
- Working playback interface.
- Personality and state selection controls.
- State/event simulation controls.
- Diagnostics and validation support.
- Documentation describing future desktop pet expansion.

Verify that the viewer and proposed AudioHole integration use the same animation playback implementation.

**Build the avatar engine once. Let AudioHole, the standalone viewer, and a future desktop pet provide different windows into the same animated personalities.**

## 16. Testing and Verification

Test the engine and standalone viewer independently wherever practical.

Verify:

- All three personalities load correctly.
- Static and animated assets display correctly.
- Transparency and frame alignment are preserved.
- Frame timing and looping operate correctly.
- One-shot reactions complete appropriately.
- Personality switching is handled safely.
- Missing and corrupt assets fail gracefully.
- No cross-personality fallback occurs.
- Reduced-motion behavior works.
- Rapid state changes do not cause crashes or inconsistent playback.
- Resource loading and playback do not cause excessive stalls.
- The viewer runs without the main AudioHole application.

Test representative application-state sequences:

- Startup → ready → healthy routing.
- Device scanning → device discovered → ready.
- Healthy routing → device disconnection → error.
- Error → recovery → healthy routing.
- Successful operation immediately followed by a critical failure.
- Personality switching during an active reaction.
- Repeated or rapidly changing notifications.
- Sustained healthy routing without unnecessary WORKING reactions.

Verify that temporary reactions return to the **latest authoritative baseline**, not a stale state.

Distinguish tests actually executed from integration behavior that remains unverified.

Do not claim successful GUI integration until integration has actually been performed and tested.

## 17. Development Workflow and Reporting

Proceed autonomously within your assigned scope.

First, preserve and checkpoint the accepted generation milestone.

Next, inspect the existing GUI and personality architecture.

Then develop the animation engine, standalone viewer, and supporting infrastructure.

Maintain reviewable commits and documentation.

Do not stop for routine implementation approvals when working within the established requirements.

Stop and request review if:

- A required change would violate your assigned scope.
- Existing architecture creates a significant unresolved compatibility issue.
- Approved assets would need substantial alteration.
- A major dependency or architectural change requires authorization.
- A critical requirement cannot be implemented reliably.

At completion, provide:

1. Approved asset checkpoint and recovery information.
2. Updated character naming and metadata.
3. Findings from GUI architecture inspection.
4. Selected animation-engine architecture and rationale.
5. Implemented module components.
6. Standalone viewer build and launch instructions.
7. Public API and integration instructions.
8. Required changes to the main application.
9. Test results and performance observations.
10. Known limitations and unresolved questions.
11. Repository branch, commit information, and handoff readiness.

Commit completed work to the assigned development branch according to repository conventions.

Do not merge into the main branch without authorization.

---

## Final Authorization

The current avatar generation milestone is **accepted**.

**Horu** is approved as the official character name for the UwU personality.

Preserve all approved artwork, animation assets, and generation infrastructure in a recoverable state.

You are authorized to inspect the existing AudioHole GUI and personality systems under **READ-ONLY access**.

You are authorized to design, implement, test, and package an integration-ready avatar animation engine and standalone personality avatar viewer **within your assigned workspace only**.

The viewer must use the production animation engine and should be architecturally prepared for future desktop pet functionality.

You are **NOT authorized** to modify the main AudioHole application or any components outside your assigned workspace.

The avatar engine must accurately reflect the application's operational state without duplicating personality logic or interfering with audio processing.

**AudioHole determines what is happening. The personality system determines how to react. The avatar engine makes the reaction visible.**

**Norm has seen this bullshit before.**

**Horu thinks this bullshit is the season finale.**

**Vanilla is a logo.**

Proceed.