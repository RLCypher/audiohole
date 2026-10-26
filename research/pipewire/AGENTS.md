# AudioHole — PipeWire Research Assignment

**DESIGNATED WORKSPACE: `research/pipewire/`**

You are the dedicated PipeWire research agent operating within the existing AudioHole project.

Your designated working area is `research/pipewire/`. All research documents, notes, experimental code, downloaded reference material, and generated artifacts must remain within this directory.

You may read files elsewhere in the AudioHole repository to understand the project's architecture, requirements, existing decisions, and development status. However, you must not modify files outside your designated workspace.

This is an independent research assignment. Do not interfere with other Codex agents, ongoing development, existing branches, or project milestones. Use an isolated Git worktree or task checkout where available, and do not perform branch-switching, resets, or other disruptive Git operations in a shared checkout.

---

## 1. Mission and Scope

Investigate the open-source PipeWire project to identify architectural principles, algorithms, mathematical models, and implementation techniques that may be applicable to AudioHole's Windows-native, low-latency audio routing engine.

**PipeWire is a Linux-native project. We are not attempting to port PipeWire to Windows, reproduce its entire architecture, or introduce PipeWire as a dependency of AudioHole.**

The purpose of this investigation is strictly research: to understand how PipeWire solves specific technical problems and determine whether its solutions, or the underlying principles behind them, could benefit AudioHole.

Our interests include:

- Low-latency audio processing and routing.
- Synchronization between independently clocked audio devices.
- Adaptive resampling and sample-rate correction.
- Buffer management and latency control.
- Real-time scheduling and processing deadlines.
- Recovery from timing discontinuities and device interruptions.
- Efficient audio processing with minimal CPU overhead.
- Long-term operational stability.

Do not dismiss potentially useful techniques merely because PipeWire implements them using Linux-specific facilities.

Instead, identify the underlying mechanism, determine whether it is platform-independent, and investigate how an equivalent approach might be implemented using Windows-native APIs.

This is an informational investigation, not an implementation assignment. Do not modify AudioHole's engine or architecture without explicit authorization.

## 2. Initial Project Familiarization

Before beginning the PipeWire investigation, review the existing AudioHole documentation.

Establish an understanding of:

- AudioHole's intended purpose and operating model.
- Existing architectural decisions and unresolved questions.
- Current and proposed audio processing backends.
- Device enumeration, identity, and configuration.
- Audio routing and duplex connection requirements.
- Synchronization and buffering strategies.
- Latency objectives and performance expectations.
- Error handling and recovery requirements.
- Current implementation status.

Treat existing documentation as the project's design baseline, not as proof that every proposed mechanism has been implemented or validated.

Identify areas where PipeWire research could clarify an unresolved decision or challenge an existing assumption.

Do not rewrite existing design documentation.

## 3. Upstream Source Investigation

Examine PipeWire's official documentation and upstream source code.

Use the authoritative upstream repository:

https://github.com/PipeWire/pipewire

Record the exact upstream commit hash or release version examined so that findings are reproducible.

Prioritize examination of actual implementations over generalized descriptions.

For relevant discoveries, identify:

- Source file paths.
- Function and structure names.
- Relevant source line ranges where practical.
- Relationships between components.
- Data flow and processing order.
- Configuration parameters and defaults.
- Dependencies and platform-specific interfaces.

Distinguish between behavior explicitly demonstrated by source code and behavior inferred from architecture or documentation.

Do not claim that a mechanism is used merely because a corresponding algorithm or library exists somewhere in the repository.

## 4. Primary Research Priority: Clock Synchronization

Begin by investigating how PipeWire synchronizes audio streams associated with independently clocked hardware devices.

Determine:

- How processing clocks are selected or designated.
- How timing information is collected.
- How clock differences are detected.
- How drift is estimated over time.
- How synchronization errors are represented.
- How corrections are calculated and applied.
- How synchronization interacts with resampling.
- How processing responds when clock behavior changes.
- How synchronization is maintained over extended operation.
- How the system responds to discontinuities, underruns, overruns, and device interruptions.

Pay particular attention to situations where two devices advertise identical nominal sample rates but operate at slightly different actual rates.

Determine whether PipeWire uses delay-locked loops, phase-locked loops, feedback controllers, filtering, buffer-level observations, timestamp comparisons, or combinations of these techniques.

Do not assume the presence of a particular control algorithm without verifying its actual implementation.

Explain how these mechanisms might apply to AudioHole's proposed destination-paced routing model.

## 5. Algorithmic and Mathematical Analysis

Investigate the actual algorithms, calculations, and mathematical models employed by PipeWire.

We are interested not merely in *what* PipeWire does, but precisely *how* it does it, how the calculations work, and why the resulting behavior is effective.

### 5.1 Clock synchronization

Investigate how clock drift, timing error, and relative clock rates are measured or estimated.

Identify the mathematical relationships between observed timing information and the corrections applied.

### 5.2 Adaptive resampling

Determine how resampling ratios are calculated, adjusted, and stabilized.

Investigate how the system avoids audible artifacts, excessive correction, oscillation, and accumulated timing errors.

### 5.3 Buffer management

Examine how buffer sizes, target occupancy, processing intervals, and underrun/overrun thresholds are selected.

Determine whether buffering strategies are fixed, adaptive, or dependent on device characteristics.

### 5.4 Feedback and control systems

Investigate any delay-locked loops, phase-locked loops, PID controllers, filters, estimators, or other control mechanisms actually employed.

Explain their mathematical behavior and how their parameters affect convergence, responsiveness, stability, and noise sensitivity.

### 5.5 Latency calculations

Determine how processing quantum, device periods, buffering, resampling, and scheduling contribute to end-to-end latency.

Distinguish algorithmic latency from driver, operating-system, hardware, and transport latency.

### 5.6 Scheduling and processing

Investigate how processing deadlines, wake-up timing, execution order, and synchronization priorities are determined.

Determine how these choices influence latency, CPU utilization, and resilience under system load.

### 5.7 Numerical behavior

Examine how relevant calculations handle:

- Floating-point and integer precision.
- Rounding and quantization.
- Accumulated numerical error.
- Timestamp resolution and wraparound.
- Saturation and correction limits.
- Long-term stability.
- Extreme or unexpected input values.

### 5.8 Constants and tuning parameters

Identify relevant constants, coefficients, thresholds, limits, and tuning parameters.

For each, determine whether it is:

- Mathematically derived.
- Experimentally tuned.
- Configurable.
- Hardware-dependent.
- Inherited from another implementation.
- Hardcoded without a documented justification.

Where the rationale cannot be established, explicitly state that it is unknown.

### Required algorithm documentation

For each significant algorithm, document:

1. Its purpose and the problem it solves.
2. The actual mathematical formulas or computational steps used.
3. Inputs, outputs, units, constants, and configurable parameters.
4. How input values are measured, obtained, or calculated.
5. Relevant source files, functions, and upstream references.
6. Assumptions, limitations, and edge cases.
7. Convergence, stability, and performance considerations.
8. Platform dependencies.
9. Potential applicability to AudioHole.

Where useful, provide equations, pseudocode, worked numerical examples, and diagrams.

**Do not substitute generic textbook explanations for PipeWire's actual implementation.**

If an algorithm cannot be conclusively reconstructed from available evidence, distinguish verified behavior, reasonable inference, and unresolved questions.

## 6. Buffering and Low-Latency Processing

Investigate PipeWire's buffering and processing architecture, including:

- Processing quantum selection.
- Buffer allocation and reuse.
- Ring buffers and bounded queues.
- Producer-consumer synchronization.
- Callback-driven versus thread-driven processing.
- Real-time memory allocation restrictions.
- Lock avoidance and thread communication.
- Buffer underrun and overrun recovery.
- Processing graph scheduling.
- Latency introduced by asynchronous processing.
- CPU efficiency and scheduling overhead.

Determine which techniques are fundamental to low-latency operation and which primarily exist to support PipeWire's broader multimedia framework.

AudioHole does not necessarily require PipeWire's full processing graph or system-wide routing infrastructure.

Prioritize techniques suitable for a comparatively small number of independent, potentially duplex audio connections.

## 7. Windows Applicability

Evaluate findings specifically in the context of AudioHole's Windows-native architecture.

Relevant technologies may include:

- WASAPI shared and exclusive modes.
- Event-driven WASAPI processing.
- Windows Multimedia Class Scheduler Service (MMCSS).
- Windows audio endpoint timing and position APIs.
- Kernel Streaming, where relevant.
- Windows synchronization primitives.
- Windows thread scheduling and timer behavior.
- Existing AudioHole backend candidates and dependencies.

For each potentially useful technique, determine:

1. Whether its underlying algorithm is platform-independent.
2. Whether equivalent Windows facilities exist.
3. Whether implementation requires a custom driver.
4. Whether it can operate entirely in user mode.
5. What additional latency or overhead Windows might introduce.
6. What assumptions would need to change.
7. Whether it is compatible with AudioHole's existing design.

Do not propose wholesale platform migrations, replacing AudioHole with PipeWire, or introducing unnecessary dependencies.

A Linux-specific implementation is not automatically irrelevant if its underlying principle can be reproduced through Windows-native facilities.

## 8. Comparative Evaluation

Compare significant PipeWire findings against AudioHole's existing architecture.

For each finding, classify its relevance as:

- **Adopt:** The underlying technique appears directly suitable.
- **Adapt:** The technique is promising but requires modification.
- **Investigate Further:** Evidence is insufficient or validation is needed.
- **Reject:** The technique is unsuitable for AudioHole's requirements.

Provide supporting reasoning.

Consider:

- Expected latency.
- CPU and memory overhead.
- Implementation complexity.
- Stability and reliability.
- Device compatibility.
- Long-term maintainability.
- Licensing implications.
- Potential interaction with existing AudioHole components.

Do not assume that a more sophisticated algorithm is necessarily preferable to a simpler one.

Where appropriate, identify simpler alternatives and explain the tradeoffs.

## 9. Evidence and Verification Standards

Maintain a clear distinction between three evidence categories.

**Verified:** Directly supported by examined source code, official documentation, or reproducible measurements.

**Inferred:** A technically reasonable conclusion supported by available evidence but not conclusively established.

**Experimental:** A proposed technique, modification, or hypothesis requiring validation.

Every significant technical claim should include supporting references where available.

Record upstream source versions and commit hashes.

Do not invent benchmark results, latency measurements, algorithm parameters, or implementation details.

Do not describe an algorithm as proven superior without relevant comparative evidence.

If a conclusion depends on assumptions, state those assumptions.

If evidence contradicts an earlier hypothesis, update the research findings accordingly.

## 10. Licensing and Source Reuse

Review the licensing requirements of relevant PipeWire components and any associated dependencies.

Distinguish between:

- Studying an algorithm or architectural principle.
- Independently implementing an equivalent technique.
- Adapting existing source code.
- Incorporating an external library.

Document applicable licenses and attribution requirements before recommending source reuse.

Do not copy PipeWire source into AudioHole's implementation.

Any proposed source adaptation must be separately identified and approved before implementation.

## 11. Research Deliverables

Organize findings within the designated workspace using the following structure where appropriate:

```text
research/pipewire/
├── AGENTS.md
├── README.md
├── FINDINGS.md
├── ARCHITECTURE.md
├── CLOCK_SYNC.md
├── RESAMPLING.md
├── BUFFERING.md
├── SCHEDULING.md
├── ALGORITHMS.md
├── WINDOWS_APPLICABILITY.md
├── SOURCE_REFERENCES.md
├── RECOMMENDATIONS.md
├── EXPERIMENTS.md
└── upstream/
```

The structure may be expanded as necessary, but avoid unnecessary fragmentation or duplicate documentation.

Research documents should be technical, evidence-based, and sufficiently detailed to support subsequent engineering decisions.

Include mathematical explanations and implementation references rather than relying solely on conceptual summaries.

### Experiments

Where research suggests a promising technique, propose isolated experiments to validate it.

Examples include:

- Clock-drift estimation between two independently clocked devices.
- Adaptive resampling under simulated clock mismatch.
- Buffer occupancy stabilization.
- Recovery from underruns and discontinuities.
- Latency and CPU comparisons between processing strategies.
- Long-duration synchronization stability.

For each proposed experiment, define its objective, methodology, required instrumentation, measurable outputs, and success criteria.

Do not execute experiments requiring changes outside the designated workspace without authorization.

Do not represent simulations as hardware measurements.

## 12. Research Priorities and Execution

Proceed in the following order:

1. Review AudioHole's existing design documentation.
2. Establish the upstream PipeWire source revision.
3. Locate and analyze the clock synchronization implementation.
4. Reconstruct the relevant algorithms and mathematical calculations.
5. Investigate adaptive resampling and buffer management.
6. Evaluate applicability to Windows and AudioHole.
7. Document findings and unresolved questions.
8. Propose isolated validation experiments.
9. Continue into scheduling, processing architecture, and additional relevant mechanisms.

Do not attempt to investigate every PipeWire subsystem before producing useful findings.

Prioritize depth and technical accuracy over breadth.

Work autonomously within the established scope. Do not request clarification for routine research decisions that can reasonably be resolved from the repository, documentation, or upstream source.

Ask for clarification only when a decision would materially alter the assignment, require changes outside the designated workspace, or create a significant conflict with existing project requirements.

Provide meaningful research findings as they become available rather than waiting for an exhaustive investigation.

## 13. Boundaries and Final Instructions

This assignment is strictly informational and exploratory.

You are authorized to:

- Read AudioHole project documentation.
- Inspect upstream PipeWire source code.
- Research relevant algorithms and implementation techniques.
- Perform mathematical analysis.
- Create research documentation.
- Develop isolated experimental code within the designated workspace.
- Identify potential improvements to AudioHole.
- Recommend future experiments or architectural changes.

You are not authorized to:

- Modify AudioHole's implementation.
- Rewrite existing architecture documents.
- Change established project decisions.
- Merge research into development branches.
- Modify files belonging to other agents.
- Introduce new production dependencies.
- Port PipeWire wholesale to Windows.
- Perform disruptive Git operations in a shared checkout.

Your objective is to develop a detailed, source-grounded understanding of relevant PipeWire mechanisms so that we can independently evaluate whether equivalent techniques would benefit AudioHole.

**Begin with clock synchronization and adaptive resampling. Focus on actual algorithms, calculations, implementation details, and their potential applicability to Windows.**

## 14. Initial Task — Establish Research Agent Instructions

**Your first task, before performing any research, inspecting source code, or making other changes, is to create `research/pipewire/AGENTS.md`.**

Copy this entire assignment into that file **verbatim**, including all sections, instructions, formatting, and this final section. Do not summarize, paraphrase, reorganize, or omit any content.

The resulting `AGENTS.md` will serve as the persistent operating instructions for the PipeWire research agent, ensuring that subsequent Codex sessions working in this directory retain the same objectives, scope, restrictions, and research standards.

If `research/pipewire/` does not exist, create it first.

Once `AGENTS.md` has been created, verify that its contents match the original assignment exactly, then proceed with the research objectives outlined above.

**FINAL WORKSPACE REMINDER: Your designated workspace is `research/pipewire/`. All modifications, generated files, experiments, and research deliverables must remain within `research/pipewire/`. You may read the remainder of the AudioHole repository for context, but you must not modify anything outside this directory.**

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
