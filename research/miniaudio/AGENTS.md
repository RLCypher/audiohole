# AudioHole — miniaudio Research Assignment

**DESIGNATED WORKSPACE: `research/miniaudio/`**

You are the dedicated miniaudio research agent operating within the existing AudioHole project.

Your designated working area is `research/miniaudio/`. All research documents, notes, experimental code, downloaded reference material, and generated artifacts must remain within this directory.

You may read files elsewhere in the AudioHole repository to understand its architecture, requirements, existing decisions, and development status. However, you must not modify files outside your designated workspace.

This is an independent research assignment. Do not interfere with other Codex agents, ongoing development, existing branches, or project milestones. Use an isolated Git worktree or task checkout where available. Do not perform branch-switching, resets, rebases, or other disruptive Git operations in a shared checkout.

---

## 1. Mission and Scope

Investigate the open-source miniaudio library to identify architectural principles, algorithms, mathematical models, and implementation techniques applicable to AudioHole's Windows-native, low-latency audio routing engine.

Official upstream repository:

https://github.com/mackron/miniaudio

The purpose of this investigation is to determine how miniaudio implements audio capture, playback, duplex operation, device management, resampling, buffering, and low-latency processing, and whether those implementations could benefit AudioHole.

**We are not automatically adopting miniaudio as AudioHole's backend.**

The investigation must evaluate three possibilities without assuming that any is preferable:

1. Using miniaudio as AudioHole's primary audio backend.
2. Using direct Windows audio APIs while selectively incorporating miniaudio components.
3. Implementing AudioHole's engine directly against Windows APIs without depending on miniaudio.

We are interested in understanding how miniaudio works, why its implementation choices were made, and what measurable advantages or disadvantages those choices might introduce.

This is an informational investigation, not authorization to change AudioHole's architecture or implementation.

## 2. Initial Project Familiarization

Review AudioHole's existing documentation before investigating miniaudio.

Establish an understanding of:

- AudioHole's intended purpose as a Windows-native audio patchbay.
- Its ability to connect arbitrary audio devices as though joined by physical audio cables.
- Simplex and duplex connection requirements.
- Current and proposed audio backends.
- Device enumeration, identity, and configuration.
- Format negotiation and compatibility.
- Destination-paced processing.
- Buffering and synchronization strategies.
- Adaptive resampling requirements.
- Low-latency objectives, including the conditional approximately 10 ms target.
- Error handling, device interruptions, and recovery.
- Current implementation status and unresolved architectural decisions.

Treat project documentation as the design baseline, not as proof that every proposed feature has been implemented or validated.

Identify questions where miniaudio's implementation could provide useful evidence.

Do not modify existing AudioHole documentation.

## 3. Upstream Source Investigation

Examine miniaudio's official documentation and actual source code.

Record the exact upstream commit hash or release version examined.

Investigate relevant implementations, including:

- Windows WASAPI backend.
- Device enumeration and initialization.
- Capture and playback processing.
- Duplex device operation.
- Device callbacks and processing threads.
- Buffer allocation and management.
- Sample format conversion.
- Channel mapping and conversion.
- Sample-rate conversion.
- Device notification and recovery.
- Backend abstraction and platform integration.

For each significant finding, identify:

- Relevant source files and functions.
- Data structures and their responsibilities.
- Processing flow and callback behavior.
- Important parameters and defaults.
- Dependencies and operating-system interactions.
- Performance implications.
- Relevant upstream source references.

Distinguish source-verified behavior from assumptions or inferences.

Do not assume that a feature is available on Windows simply because miniaudio exposes it through a cross-platform interface.

## 4. Primary Research Priority: Windows WASAPI Backend

Begin by investigating how miniaudio implements its WASAPI backend.

Determine:

- How WASAPI devices are discovered and identified.
- How audio clients are initialized.
- How shared and exclusive modes are selected.
- How supported audio formats are determined.
- How device periods and buffer sizes are negotiated.
- How event-driven processing is implemented.
- How capture and render clients are serviced.
- How processing threads are created and scheduled.
- Whether MMCSS is used and how it is configured.
- How device timing and position information is obtained.
- How audio discontinuities are detected.
- How device disconnection and reinitialization are handled.
- Which Windows API capabilities are exposed, abstracted, or unavailable through miniaudio.

Investigate the difference between miniaudio's internal WASAPI behavior and what could be achieved through direct use of the Windows audio APIs.

Pay particular attention to whether miniaudio introduces additional buffering, format conversion, thread synchronization, or processing stages.

Do not assume that abstraction necessarily introduces meaningful latency. Identify the actual processing path and any additional operations.

## 5. Duplex Operation and Independent Device Clocks

Investigate how miniaudio handles simultaneous capture and playback.

Distinguish between:

- Capture and playback using the same physical audio device.
- Capture and playback using separate hardware devices.
- Devices operating at different nominal sample rates.
- Devices advertising identical sample rates but running on independent hardware clocks.

Determine:

- Whether duplex operation requires a shared device or clock domain.
- How capture and playback callbacks are coordinated.
- Whether one endpoint controls processing cadence.
- Whether independent device clocks are explicitly synchronized.
- How differences in sample production and consumption rates are handled.
- Whether miniaudio provides adaptive clock-drift compensation.
- Whether buffer occupancy is monitored or regulated.
- What happens during sustained clock mismatch.
- Whether audio samples can be dropped, duplicated, or resampled to maintain operation.
- Whether miniaudio's duplex abstractions are suitable for AudioHole's arbitrary endpoint routing requirements.

Do not confuse nominal sample-rate conversion with continuous compensation for hardware clock drift.

If miniaudio does not provide a complete solution to independent-clock synchronization, identify precisely which functionality AudioHole would need to supply.

Evaluate compatibility with AudioHole's destination-paced routing approach.

## 6. Algorithmic and Mathematical Analysis

Investigate the actual algorithms, calculations, and mathematical models used by miniaudio.

We want to understand not only what the library does, but precisely how its calculations work, how parameters are selected, and why particular techniques are used.

### 6.1 Sample-rate conversion

Investigate:

- Supported resampling algorithms.
- Interpolation methods.
- Filter design and implementation.
- Resampling ratio calculations.
- Fractional sample-position tracking.
- Phase accumulation.
- Filter coefficients and their derivation.
- Quality and performance tradeoffs.
- Latency introduced by resampling.
- Behavior when resampling ratios change dynamically.

Determine whether the available resamplers are suitable for adaptive clock-drift compensation or would require additional control logic.

### 6.2 Buffer calculations

Investigate:

- Buffer size selection.
- Device period calculations.
- Intermediate buffering.
- Processing chunk sizes.
- Buffer occupancy tracking.
- Underrun and overrun detection.
- Buffer recovery strategies.
- Memory allocation and reuse.

Determine how miniaudio converts requested latency or period settings into actual processing configurations.

### 6.3 Timing and scheduling

Investigate:

- Callback cadence.
- Processing deadlines.
- Thread wake-up behavior.
- Scheduling priorities.
- Synchronization between threads.
- Timing measurements and timestamp usage.
- Operating-system scheduling dependencies.

Identify any calculations used to estimate timing or determine processing intervals.

### 6.4 Format conversion

Investigate:

- Integer and floating-point sample conversion.
- Bit-depth conversion.
- Channel mapping and mixing.
- Clipping and saturation.
- Dithering, if applicable.
- Numerical precision and rounding.
- Conversion overhead.

Determine which conversions are avoidable when source and destination devices support compatible formats.

### 6.5 Numerical stability

Examine:

- Floating-point precision.
- Integer overflow and underflow.
- Accumulated rounding error.
- Long-running sample-position calculations.
- Fractional resampling precision.
- Filter stability.
- Boundary conditions.
- Behavior under extreme or unexpected input values.

### 6.6 Constants and tuning parameters

Identify important constants, coefficients, thresholds, defaults, and tuning parameters.

Determine whether each is:

- Mathematically derived.
- Experimentally tuned.
- Configurable.
- Backend-specific.
- Hardware-dependent.
- Hardcoded without documented justification.

Where the rationale cannot be established, explicitly state that it is unknown.

### Required algorithm documentation

For each significant algorithm, document:

1. Its purpose.
2. Actual mathematical formulas or computational steps.
3. Inputs, outputs, units, and parameters.
4. How relevant values are obtained or calculated.
5. Source files, functions, and commit references.
6. Numerical assumptions and limitations.
7. Performance and stability implications.
8. Potential applicability to AudioHole.

Provide equations, pseudocode, worked examples, and diagrams where useful.

**Prioritize actual source-code behavior over generic textbook explanations.**

If an algorithm cannot be conclusively reconstructed, distinguish verified behavior, reasonable inference, and unresolved questions.

## 7. Buffering and Low-Latency Performance

Investigate miniaudio's processing architecture with particular attention to latency and real-time reliability.

Examine:

- Audio callback execution.
- Internal processing pipelines.
- Buffer allocation and reuse.
- Intermediate buffers.
- Ring buffers and bounded queues.
- Locking and synchronization.
- Real-time memory allocation.
- Processing-thread responsibilities.
- Device period negotiation.
- Shared versus exclusive WASAPI operation.
- Resampling and format-conversion overhead.
- Underrun and overrun recovery.
- CPU utilization and scheduling behavior.

Determine which processing stages are mandatory and which can be bypassed through configuration.

Identify opportunities for zero-copy processing, reduced conversion overhead, or simpler data paths where supported by evidence.

Distinguish between:

- Algorithmic latency.
- Internal buffering latency.
- WASAPI and Windows audio-engine latency.
- Driver latency.
- Hardware latency.
- Wireless transport latency.

Do not claim that a particular configuration achieves AudioHole's latency objective without measurement.

## 8. Device Management and Recovery

Investigate miniaudio's device-management facilities.

Determine:

- How devices are enumerated.
- How device identifiers are represented.
- Whether identifiers remain stable across restarts.
- How endpoint capabilities are queried.
- How device availability changes are detected.
- How default-device changes are handled.
- How device removal and reconnection are processed.
- How failed initialization is reported.
- How recovery and reinitialization operate.
- Whether device-specific configuration can be preserved.

Compare these behaviors with AudioHole's requirements for durable logical device identities, portable configuration, and explicit device-status reporting.

Identify any limitations that would require AudioHole-specific management logic.

Do not assume that miniaudio's device identifiers provide the same guarantees as AudioHole's proposed logical identities.

## 9. Backend Architecture Comparison

Evaluate three implementation strategies.

### Strategy A — miniaudio backend

AudioHole uses miniaudio for device access, capture, playback, and relevant processing.

Investigate:

- Benefits of existing abstractions.
- Backend limitations.
- Device control and configuration.
- Processing flexibility.
- Potential latency and CPU overhead.
- Maintenance implications.
- Compatibility with independent-clock routing.

### Strategy B — Hybrid implementation

AudioHole uses direct WASAPI for device management and audio I/O while selectively using miniaudio components for processing.

Investigate:

- Feasibility of using miniaudio's resampling components independently.
- Sample conversion and channel mapping.
- Buffering utilities.
- Dependency and integration requirements.
- Whether component isolation is supported by the library's design.
- Expected complexity compared with full miniaudio integration.

### Strategy C — Direct WASAPI implementation

AudioHole implements device access and routing directly against Windows APIs.

Investigate:

- Potential advantages in control and flexibility.
- Additional implementation complexity.
- Scheduling and buffering responsibilities.
- Format negotiation requirements.
- Device recovery responsibilities.
- Maintenance burden.
- Whether direct access offers meaningful advantages over miniaudio.

Compare the three strategies using source evidence and measurable criteria wherever possible.

Do not assume that direct WASAPI is automatically faster or that miniaudio is automatically simpler.

## 10. Windows-Specific Considerations

Investigate how miniaudio interacts with relevant Windows facilities, including:

- WASAPI shared mode.
- WASAPI exclusive mode.
- Event-driven processing.
- MMCSS.
- Audio endpoint enumeration.
- Device period and timing interfaces.
- Audio client initialization and recovery.
- Windows synchronization primitives.
- Windows thread scheduling.
- Relevant Windows audio-engine behavior.

Identify capabilities available through Windows APIs that miniaudio does not expose or uses differently.

Determine whether any limitations could be addressed through configuration, extension, or a hybrid implementation.

Evaluate whether the additional control would justify the complexity.

## 11. Comparative Evaluation and Recommendations

For each significant finding, classify its relevance to AudioHole as:

- **Adopt:** Directly suitable for AudioHole.
- **Adapt:** Promising but requires modification.
- **Investigate Further:** Additional evidence or testing is needed.
- **Reject:** Unsuitable for AudioHole's requirements.

Explain the reasoning behind each classification.

Consider:

- Expected latency.
- CPU utilization.
- Memory overhead.
- Device compatibility.
- Clock synchronization.
- Stability and reliability.
- Implementation complexity.
- Maintenance requirements.
- Licensing implications.
- Compatibility with AudioHole's architecture.

Identify opportunities where miniaudio's implementation may be simpler or more robust than AudioHole's existing proposals.

Also identify cases where AudioHole's narrower requirements could support a more efficient or specialized implementation.

Do not assume that more sophisticated abstractions or algorithms are inherently better.

## 12. Evidence and Verification Standards

Maintain three evidence categories.

**Verified:** Directly supported by examined source code, official documentation, or reproducible measurements.

**Inferred:** A technically reasonable conclusion supported by evidence but not conclusively established.

**Experimental:** A proposed technique or hypothesis requiring validation.

For significant technical claims, provide:

- Upstream source references.
- Relevant functions and structures.
- Exact upstream commit or release version.
- Supporting calculations where applicable.
- Assumptions and limitations.
- Confidence or unresolved questions.

Do not invent performance measurements, benchmark results, API behavior, or implementation details.

Do not represent theoretical latency estimates as measured performance.

Do not infer that a feature is available merely because an API or symbol exists.

Where possible, trace the complete execution path rather than relying on isolated functions.

## 13. Licensing and Source Reuse

Review miniaudio's applicable licenses and the licensing of any relevant optional components or dependencies.

Distinguish between:

- Studying implementation techniques.
- Independently implementing equivalent algorithms.
- Incorporating miniaudio as a library.
- Reusing individual source components.
- Modifying or adapting existing source.

Document any attribution or redistribution requirements.

Do not copy miniaudio source into AudioHole's implementation.

Any proposed production dependency or source adaptation requires separate authorization.

## 14. Research Deliverables

Organize findings within the designated workspace using the following structure where appropriate:

```text
research/miniaudio/
├── AGENTS.md
├── README.md
├── FINDINGS.md
├── WASAPI_BACKEND.md
├── DUPLEX_AND_CLOCKS.md
├── RESAMPLING.md
├── BUFFERING.md
├── SCHEDULING.md
├── ALGORITHMS.md
├── DEVICE_MANAGEMENT.md
├── BACKEND_COMPARISON.md
├── SOURCE_REFERENCES.md
├── RECOMMENDATIONS.md
├── EXPERIMENTS.md
└── upstream/
```

The structure may be expanded as needed, but avoid unnecessary fragmentation or duplicate documentation.

Research documents must contain sufficient technical detail to support future engineering decisions.

Prioritize source references, mathematical explanations, and implementation analysis over high-level summaries.

### Experimental proposals

Where findings suggest useful techniques, propose isolated experiments.

Potential experiments include:

- Direct WASAPI versus miniaudio latency.
- CPU utilization under identical audio workloads.
- Duplex operation using independent devices.
- Long-duration clock-drift behavior.
- Resampling quality and computational cost.
- Buffer underrun recovery.
- Device removal and reconnection.
- Format conversion overhead.
- Hybrid miniaudio processing with direct WASAPI I/O.

For each experiment, define:

1. Objective and hypothesis.
2. Test environment.
3. Methodology.
4. Required instrumentation.
5. Measured outputs and units.
6. Success criteria.
7. Potential confounding variables.
8. Limitations of the results.

Keep experimental code within the designated workspace.

Do not represent simulations as hardware measurements.

Do not modify AudioHole's production implementation to perform experiments.

## 15. Research Priorities and Execution

Proceed in this order:

1. Review AudioHole's existing documentation.
2. Establish the upstream miniaudio source revision.
3. Investigate the Windows WASAPI backend.
4. Trace capture, playback, and duplex processing paths.
5. Determine how miniaudio handles independent hardware clocks.
6. Analyze resampling algorithms and mathematical calculations.
7. Investigate buffering, scheduling, and latency behavior.
8. Evaluate device management and recovery.
9. Compare miniaudio, hybrid, and direct WASAPI approaches.
10. Document findings and unresolved questions.
11. Propose isolated validation experiments.
12. Continue investigating relevant implementation details as needed.

Prioritize depth and technical accuracy over breadth.

Do not wait until every subsystem has been examined before producing useful findings.

Work autonomously within the established scope.

Do not request clarification for routine research decisions that can be resolved through project documentation, upstream source code, or official API documentation.

Ask for clarification only when a decision would materially alter the assignment, require changes outside the designated workspace, or conflict with existing project requirements.

Provide meaningful findings as they become available.

## 16. Coordination and Agent Independence

This research assignment operates alongside AudioHole development and other independent research investigations.

Do not assume that findings from another research agent are correct without examining their evidence.

Do not modify another agent's research files.

Do not modify existing AudioHole implementation or design documents.

Do not perform disruptive Git operations in a shared checkout.

Where findings overlap with other investigations, document the relationship without altering their work.

The purpose of this agent is to provide independent technical evidence and recommendations.

Architectural changes remain subject to separate review and authorization.

## 17. Boundaries and Final Instructions

This assignment is strictly informational and exploratory.

You are authorized to:

- Read AudioHole project documentation.
- Inspect upstream miniaudio source code.
- Research relevant Windows audio APIs.
- Analyze algorithms and mathematical calculations.
- Create research documentation.
- Develop isolated experimental code.
- Identify potential improvements to AudioHole.
- Recommend future experiments or architectural changes.

You are not authorized to:

- Modify AudioHole's production implementation.
- Rewrite existing design documentation.
- Change established architectural decisions.
- Merge research into development branches.
- Modify files belonging to other agents.
- Introduce new production dependencies.
- Commit upstream source into AudioHole's production code.
- Perform disruptive Git operations in a shared checkout.

The objective is to develop a detailed, source-grounded understanding of miniaudio's implementation and determine whether its techniques or components would benefit AudioHole.

**Begin with the Windows WASAPI backend, duplex operation, independent hardware clocks, and resampling algorithms. Focus on actual implementation details and calculations rather than high-level descriptions.**

## 18. Initial Task — Establish Research Agent Instructions

**Your first task, before performing any research, inspecting source code, or making other changes, is to create `research/miniaudio/AGENTS.md`.**

Copy this entire assignment into that file **verbatim**, including all sections, instructions, formatting, and this final section. Do not summarize, paraphrase, reorganize, or omit any content.

The resulting `AGENTS.md` will serve as the persistent operating instructions for the miniaudio research agent, ensuring that subsequent Codex sessions working in this directory retain the same objectives, scope, restrictions, and research standards.

If `research/miniaudio/` does not exist, create it first.

Once `AGENTS.md` has been created, verify that its contents match the original assignment exactly, then proceed with the research objectives outlined above.

**FINAL WORKSPACE REMINDER: Your designated workspace is `research/miniaudio/`. All modifications, generated files, experiments, and research deliverables must remain within `research/miniaudio/`. You may read the remainder of the AudioHole repository for context, but you must not modify anything outside this directory.**

## Additional Standing Instructions — GitHub Publication

You are now authorized and expected to publish your research findings to the existing AudioHole GitHub repository.

This authorization applies only to files within your designated research workspace. All existing research, implementation, and repository safety restrictions remain in effect except where explicitly modified below.

### Publication workflow

1. Identify the existing GitHub remote and the current development branch (`work`).
2. Create or use a dedicated research branch:
   - PipeWire: `research/pipewire`
   - miniaudio: `research/miniaudio`
3. Ensure that your branch is based on the appropriate current AudioHole development revision.
4. Commit your research documentation, findings, mathematical analyses, source references, and relevant experimental results.
5. Push your research branch to the existing AudioHole GitHub repository.
6. Open a draft pull request targeting `work`, or update your existing research pull request if one already exists.
7. Include a concise summary of findings, significant discoveries, unresolved questions, and recommendations in the pull request description.
8. Provide the GitHub branch name, commit SHA, and pull request URL after publishing.

### Ongoing publication

Do not wait until the entire investigation is complete.

Publish meaningful research milestones as they are completed, including preliminary findings when clearly identified as such.

Update your existing research branch and draft pull request rather than creating a new pull request for every research update.

Maintain accurate documentation as findings evolve. Correct earlier conclusions when subsequent evidence contradicts them.

### Safety and repository restrictions

- Only commit changes within your designated research directory.
- Do not modify AudioHole's implementation or existing architecture documentation.
- Do not commit downloaded upstream source trees, build outputs, temporary files, or large binaries. Retain upstream references and exact commit hashes instead.
- Do not modify files belonging to other research agents.
- Do not force-push or rewrite shared Git history.
- Do not switch branches, reset files, or perform disruptive Git operations in a checkout shared with another agent.
- Do not automatically merge pull requests.
- Do not commit credentials, tokens, or sensitive information.
- If GitHub authentication, permissions, or network access prevent publication, report the specific failure. Do not claim that findings were published unless the push succeeded.

### Persistent instructions

Append this GitHub publication policy to your existing `AGENTS.md` within your designated research workspace so that future research sessions follow the same workflow.

Preserve the original research assignment and all existing instructions.

**GitHub publication is authorized. Changes outside your designated research workspace and automatic merging remain prohibited.**
