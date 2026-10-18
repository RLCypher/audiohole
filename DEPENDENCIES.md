# AudioHole v0.1 — Dependencies and Prior Art

**Document:** `DEPENDENCIES.md`  
**Status:** Research and architectural reference  
**Primary language:** C++  
**Target platform:** Windows 10/11, 64-bit  
**Project version:** 0.1

---

## 1. Purpose

AudioHole is a clean-sheet rewrite of an earlier C++ proof of concept.

The original implementation demonstrated that the core concept was technically feasible using a relatively small collection of open-source libraries.

Although the original source code is no longer available, the libraries and approaches used during its development provide useful starting points for the new implementation.

**The purpose of this document is to identify what previously worked, what can be reused, and where better alternatives may exist.**

Previous dependency choices are not binding architectural decisions.

Developers and coding agents are encouraged to evaluate alternative implementations when they offer meaningful improvements in performance, reliability, maintainability, functionality, or diagnostic transparency.

However, alternatives should be justified by concrete technical benefits rather than novelty or unnecessary complexity.

This document is advisory. It identifies known starting points and candidates for investigation, not a mandatory dependency list.

---

## 2. Original Proof-of-Concept Foundation

### 2.1 C++

The original AudioHole was implemented almost entirely in C++.

C++ remains the preferred implementation language because of its suitability for low-latency audio, direct Windows API access, deterministic resource management, and efficient execution.

The rewrite should use modern C++ practices, including RAII, explicit resource ownership, and clear separation between real-time and non-real-time operations.

A different language or runtime should require a compelling technical justification.

### 2.2 miniaudio and Direct WASAPI

**miniaudio:** https://github.com/mackron/miniaudio  
**Windows Core Audio / WASAPI:** https://learn.microsoft.com/en-us/windows/win32/coreaudio/wasapi

The original AudioHole proof of concept relied heavily on miniaudio for audio device interaction and stream handling. It provided a lightweight, practical foundation for establishing audio connections without implementing the entire audio subsystem manually.

miniaudio remains a strong candidate for the rewrite. However, **direct WASAPI integration should be strongly explored as a potentially superior approach for some or all of AudioHole's audio operations.**

#### Why Direct WASAPI Deserves Serious Consideration

AudioHole's primary objective is to connect arbitrary Windows audio devices with minimal unnecessary processing, low latency, and predictable behavior.

Direct WASAPI provides access to Windows audio functionality that may be simplified, abstracted, or not exposed through a general-purpose audio library.

Potential advantages include:

- **Precise format negotiation:** Explicitly query and attempt specific sample rates, sample representations, valid bit depths, channel configurations, and operating modes.
- **Advanced capability probing:** Test configurations beyond those normally enumerated, capturing exact API responses and failure conditions.
- **Reduced unnecessary conversion:** Investigate whether compatible devices can operate with matching formats, avoiding redundant conversion stages.
- **Detailed buffer control:** Examine device periods, supported buffer configurations, and low-latency shared or exclusive operation.
- **Diagnostic transparency:** Preserve exact HRESULT values, initialization parameters, and driver behavior rather than relying solely on generalized library errors.
- **Device inspection:** Access Windows endpoint properties, state changes, and identification information needed for portable device matching.
- **Reduced abstraction:** Eliminate intermediate software layers where they introduce measurable overhead or prevent required functionality.

Direct WASAPI does not inherently guarantee lower latency or better audio quality than miniaudio. Both may ultimately use the same Windows audio facilities.

Likewise, matching audio formats does not eliminate the need to compensate for independently clocked devices.

Any claimed performance advantage must be demonstrated through testing rather than assumed.

#### Required Architectural Evaluation

Before finalizing the audio engine, evaluate three approaches:

1. **miniaudio-centric:** Use miniaudio for normal streaming, with direct WASAPI for capability probing and Windows-specific diagnostics.
2. **Hybrid:** Use miniaudio and native WASAPI streaming implementations behind a common audio backend interface, selecting the appropriate implementation according to device capabilities and requirements.
3. **WASAPI-centric:** Implement capture, playback, format negotiation, and stream management directly against Windows Core Audio, using supporting libraries only where beneficial.

Evaluate these approaches against:

- End-to-end and AudioHole-attributable latency.
- CPU and memory overhead.
- Stability under sustained operation.
- Behavior with independent device clocks.
- Accuracy and completeness of capability probing.
- Shared and exclusive mode support.
- Quality of diagnostic information.
- Complexity and maintainability.
- Ability to support multiple concurrent duplex connections.
- Recovery from device disconnection and reenumeration.

#### Implementation Guidance

Do not assume that the original miniaudio-based approach is optimal merely because it worked previously.

Equally, do not replace miniaudio with a custom WASAPI implementation solely to eliminate a dependency.

**Direct WASAPI should be treated as a serious first-class architectural candidate, not merely a fallback for missing miniaudio features.**

The final architecture should favor whichever approach best satisfies AudioHole's requirements with the least unnecessary complexity.

A hybrid implementation is acceptable if it offers meaningful advantages.

Document the selected approach and its rationale in `ARCHITECTURE.md` and `DECISIONS.md`.

### 2.3 nlohmann/json

**Repository:** https://github.com/nlohmann/json  
**License:** MIT  
**Status:** Previously used; strong candidate

The original implementation used JSON for human-readable configuration and device information.

This approach aligns with one of AudioHole's fundamental design requirements: configuration must remain understandable and manually editable.

nlohmann/json provides a header-only C++ implementation with straightforward serialization and parsing.

Potential responsibilities include:

- Reading and writing configuration files.
- Device capability records.
- Profile serialization.
- Device database summaries.
- Structured diagnostic exports.
- Schema validation support.

JSON operations must remain outside real-time audio callbacks.

Alternatives may be considered, but they must preserve readable JSON files and provide a meaningful advantage.

---

## 3. Build System

### 3.1 CMake

**Website:** https://cmake.org/  
**Status:** Preferred build system

CMake is the preferred build system for the AudioHole rewrite.

Responsibilities include:

- C++ project configuration.
- Dependency integration.
- Windows builds.
- Debug and release configurations.
- Automated test integration.
- Reproducible build configuration.

The project should support building with commonly available Windows C++ toolchains without requiring a proprietary IDE project format.

Dependency versions should be pinned or otherwise made reproducible.

---

## 4. Candidate Open-Source Components

The following libraries are candidates for evaluation.

Their inclusion does not imply selection.

### 4.1 GUI Frameworks

| Library | Potential role | Considerations |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Lightweight GUI, diagnostic panels, device inspectors | Immediate-mode design; desktop integration requires additional work |
| [Qt](https://www.qt.io/) | Conventional desktop GUI, dialogs, system tray | Mature framework; larger dependency and licensing considerations |
| [wxWidgets](https://github.com/wxWidgets/wxWidgets) | Native-style desktop GUI | Traditional application controls and Windows integration |

The selected GUI framework should provide:

- Responsive device and profile management.
- Convenient system tray integration.
- Live diagnostic output.
- Clear dialogs and confirmation prompts.
- Minimal interference with the audio engine.
- Reasonable deployment size.
- Maintainable C++ integration.

The audio engine must not depend on the GUI framework.

### 4.2 Logging and Formatting

| Library | Potential role | Considerations |
|---|---|---|
| [spdlog](https://github.com/gabime/spdlog) | Structured and asynchronous logging | Must be isolated from real-time callbacks |
| [fmt](https://github.com/fmtlib/fmt) | Efficient text formatting | May be unnecessary where standard C++ facilities suffice |

AudioHole requires comprehensive diagnostic logging, particularly during device probing.

Any selected logging system must support persistent records, configurable verbosity, and safe handling of high-frequency events.

### 4.3 Automated Testing

| Library | Potential role | Considerations |
|---|---|---|
| [GoogleTest](https://github.com/google/googletest) | Unit and integration testing | Established C++ testing ecosystem |
| [Catch2](https://github.com/catchorg/Catch2) | Unit and integration testing | Alternative with straightforward integration |

AudioHole should select one primary testing framework rather than maintaining redundant frameworks without justification.

Testing should cover configuration validation, device identity matching, routing decisions, failover policies, and simulated device behavior.

### 4.4 Audio Processing

Additional audio processing libraries may be considered if they provide measurable advantages.

Potential responsibilities include:

- High-quality sample-rate conversion.
- Channel conversion.
- Clock-drift compensation.
- Format conversion.
- Efficient buffer handling.

AudioHole should first evaluate capabilities already available through the selected audio backend.

Additional DSP dependencies must be justified by functionality, performance, or audio quality improvements.

---

## 5. Windows Native APIs

AudioHole is a Windows-focused application.

Direct integration with Windows APIs is appropriate when it provides better functionality, diagnostics, or control than a general-purpose abstraction.

Relevant APIs include:

### 5.1 Windows Core Audio

- WASAPI.
- `IAudioClient`.
- `IAudioClient3`.
- `IAudioCaptureClient`.
- `IAudioRenderClient`.
- `IAudioClock`.
- `IMMDeviceEnumerator`.
- `IMMNotificationClient`.
- Windows audio endpoint property interfaces.

Potential uses include:

- Audio capture and playback.
- Capability querying.
- Shared and exclusive stream initialization.
- Low-latency configuration.
- Buffer and clock inspection.
- Device enumeration.
- Endpoint state notifications.

### 5.2 Device Identification

Windows device identification facilities may provide:

- Hardware identifiers.
- Device instance properties.
- Interface information.
- USB identification details.
- Driver metadata.
- Device arrival and removal notifications.

These facilities may assist AudioHole's portable device identity system.

No single Windows identifier should be assumed durable across operating system reinstalls or hardware changes.

### 5.3 Scheduling and Performance

Relevant Windows facilities include:

- Multimedia Class Scheduler Service (MMCSS).
- High-resolution timing facilities.
- Thread priority management.
- Performance counters.

The application should prefer established Windows multimedia scheduling mechanisms over arbitrary real-time priority escalation.

### 5.4 Desktop Integration

Windows APIs may provide:

- System tray integration.
- Application notifications.
- File dialogs.
- Process lifecycle handling.
- Power and session notifications.

These capabilities may be accessed directly or through the selected GUI framework.

---

## 6. Dependency Evaluation Criteria

Dependencies should be evaluated according to:

1. Real-time safety and predictable performance.
2. Ability to expose required Windows audio capabilities.
3. Licensing compatibility with AudioHole.
4. Ease of integration with CMake.
5. Source availability and maintainability.
6. Minimal runtime dependencies.
7. Documentation quality.
8. Ease of debugging.
9. Binary size and memory overhead.
10. Long-term portability and extensibility.

A dependency that simplifies development but obscures critical audio behavior may be unsuitable for the relevant subsystem.

A dependency that offers low-level control but introduces substantial unnecessary complexity may also be unsuitable.

**The preferred solution is the simplest implementation that fully satisfies AudioHole's functional and diagnostic requirements.**

---

## 7. Integration Rules

The following principles apply to third-party components:

- Pin dependencies to known versions or commits.
- Record dependency versions in build metadata.
- Preserve required copyright and license notices.
- Avoid undocumented modifications to third-party source code.
- Keep third-party components behind clear project interfaces where practical.
- Do not perform filesystem, JSON, or blocking logging operations in real-time audio callbacks.
- Avoid dependencies requiring cloud services for ordinary operation.
- Keep the audio engine independent of the selected GUI framework.
- Avoid multiple libraries with substantially overlapping responsibilities.
- Ensure dependencies can be built and distributed consistently.

Dependencies must not create opaque user-facing configuration or prevent manual inspection of application state.

---

## 8. Components AudioHole Should Own

AudioHole should implement and maintain its own application-specific logic for:

- Logical device identity and portable matching.
- Profile and hole configuration.
- Duplex connection coordination.
- Routing supervision.
- Failover policy and availability detection.
- Capability probing strategy.
- Capability evidence classification.
- Device database organization.
- Connection-count limits and warnings.
- User-facing diagnostic semantics.
- Configuration validation and migration.

Third-party libraries may provide lower-level mechanisms, but AudioHole must retain control of these behaviors.

---

## 9. Components to Avoid by Default

Avoid introducing:

- Electron or an embedded browser for the desktop interface.
- Mandatory cloud infrastructure.
- Proprietary configuration databases.
- Kernel-mode drivers without a demonstrated requirement.
- Heavy multimedia frameworks when smaller alternatives suffice.
- Multiple competing audio engines without a clear architectural purpose.
- Dependencies used only for trivial functionality already available through C++ or Windows.
- Unnecessary background services or elevated privileges.

These are preferences rather than unconditional prohibitions.

Exceptions require documented technical justification.

---

## 10. Architectural Questions Requiring Evaluation

Before finalizing the architecture, determine:

### Audio Backend

- Can miniaudio satisfy AudioHole's latency and reliability requirements?
- Which operations benefit from direct WASAPI integration?
- Does a hybrid approach introduce unnecessary complexity?
- Are there measurable performance differences between miniaudio and direct WASAPI under equivalent configurations?
- Can both approaches expose sufficient information for troubleshooting?

### Device Probing

- Which probing operations require direct WASAPI?
- How should experimentally discovered formats be distinguished from Windows-reported capabilities?
- How can false positives be minimized?
- How should shared-mode conversion be identified?
- How should probing coordinate access with active audio routes?

### GUI

- Which framework best supports conventional Windows application behavior?
- How should the live diagnostic console be implemented?
- Can the GUI be opened and closed without interrupting routing?
- How should the application integrate with the system tray?

### Logging

- Is a dedicated logging library necessary?
- How should real-time diagnostic events be buffered?
- How should persistent logs be organized and rotated?

### Testing

- Which testing framework should be adopted?
- How should simulated audio endpoints and device failures be represented?
- Which performance characteristics require physical hardware testing?

---

## 11. Architectural Authority

This document is a reference, not an implementation mandate.

The authoritative responsibilities of the project documents are:

- **`SPEC.md`:** Required application behavior.
- **`DEVICE_PROBING.md`:** Discovery, experimental probing, and capability evidence requirements.
- **`ARCHITECTURE.md`:** Selected implementation architecture.
- **`CONFIG_FORMAT.md`:** Persistent file formats and schemas.
- **`DECISIONS.md`:** Major architectural decisions and their rationale.
- **`DEPENDENCIES.md`:** Prior art, candidate libraries, and dependency evaluation guidance.
- **`AGENTS.md`:** Standing development instructions.

Where a dependency conflicts with a functional requirement, the functional requirement takes precedence.

Final dependency choices should be recorded in the architecture and decision documentation.

---

## 12. Guiding Principle

The original AudioHole demonstrated that the concept could work using C++, miniaudio, and JSON.

The rewrite should build upon that experience without being constrained by it.

**Use existing libraries where they make AudioHole better. Use direct Windows APIs where they provide meaningful advantages. Write custom code where AudioHole's requirements demand it.**

The objective is not to minimize the number of dependencies at any cost, nor to maximize the amount of custom code.

The objective is to produce a reliable, efficient, transparent, maintainable audio-routing application that behaves as closely as practical to a physical duplex audio cable.