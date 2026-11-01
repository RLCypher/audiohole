<p align="center">
  <img src="assets/branding/audiohole-logo.png" width="224" alt="AudioHole logo: mint output and pink microphone surfaces joined by a wireframe audio conduit">
</p>

# audiohole

AudioHole is an open-source Windows audio routing utility that connects audio devices through configurable simplex and duplex paths. Lightweight, portable, and built for people who just want their holes to work.

## Overview

AudioHole is designed to make routing audio between Windows devices simple, reliable, and transparent.

Whether you're connecting a microphone to a virtual audio device, routing audio between applications through existing endpoints, or managing a more complicated setup, AudioHole provides a straightforward way to connect sources and destinations without unnecessary complexity.

No custom kernel drivers, complicated virtual mixers, or mysterious configuration files. Just connect the right devices and let AudioHole handle the routing.

## Features

*Planned for v0.1. Not all features are implemented yet.*

- **Simplex and duplex routing:** Create one-way or bidirectional connections between Windows audio endpoints.
- **Multiple simultaneous connections:** Run several independent audio paths at once.
- **Automatic device discovery:** Detect available audio devices and identify their capabilities.
- **Persistent device assignments:** Remember devices and restore connections when they become available.
- **Profiles:** Save, load, and switch between different routing configurations.
- **Automatic recovery:** Handle device disconnections, reconnections, and interruptions.
- **Flexible startup behavior:** Start routing manually, immediately, or after a configurable delay.
- **System tray integration:** Keep AudioHole running without leaving the main window open.
- **Signal activity monitoring:** Temporarily display source activity indicators for setup and troubleshooting.
- **Test beep:** Send a short test signal to a selected destination to verify that audio reaches the intended device.
- **Portable configuration:** Store settings and profiles in human-readable JSON files.

## How It Works

AudioHole connects existing Windows audio endpoints through configurable connections called **holes**.

### Simplex Routing

A simplex hole carries audio in one direction.

`Source → Destination`

For example, audio captured from a microphone can be forwarded to another audio endpoint.

### Duplex Routing

A duplex hole contains two independently configurable directional paths.

`Device A ⇄ Device B`

Each direction can have its own source, destination, and audio settings.

This allows bidirectional communication between devices without requiring two separately managed connections.

### Multiple Holes

AudioHole supports multiple independent connections running simultaneously, allowing more complex setups while keeping each audio path individually manageable.

## Platform and Requirements

- Windows 10 or Windows 11 (initial tested baseline planned: Windows 10 22H2 and supported Windows 11 releases; capabilities detected at runtime)
- 64-bit (x64)
- Native C++ application
- CMake build system
- Standard user permissions for normal operation
- No custom kernel drivers required
- No cloud services, accounts, or internet connection required for normal operation

AudioHole works with existing Windows audio endpoints. It does not create virtual audio devices or replace device drivers.

## Design Philosophy

**Simple by design.** Audio routing shouldn't require learning an entire digital audio workstation.

**Transparent configuration.** Settings should be human-readable, portable, and easy to back up or modify.

**Reliable connections.** AudioHole should recover from interruptions without silently connecting the wrong devices.

**User control.** Technically supported configurations should generally be allowed, with clear warnings about potential problems.

**Minimal overhead.** Keep resource consumption and added audio latency as low as reasonably possible.

## Project Status

**Pre-alpha — Early development**

AudioHole is currently under development. The initial focus is establishing a reliable Windows audio routing engine and building the application around it.

There are no stable releases available yet.

### Development Roadmap

1. Establish the application architecture and build system.
2. Implement basic simplex audio routing.
3. Validate independent-clock synchronization, latency, backend comparison, and exclusive-mode behavior.
4. Establish recovery and portable device identity, then expand to duplex and multiple holes.
5. Add profiles and configurable startup behavior.
6. Develop the graphical interface and system tray integration.
7. Implement signal activity monitoring and output test beeps.
8. Test performance, reliability, and compatibility across different hardware configurations.

## Building

The Milestone 0 foundation contains a Windows x64 developer executable, COM initialization support, and GoogleTest/CTest targets. It does not route audio yet.

See [BUILDING.md](BUILDING.md) for prerequisites, Debug/Release build and test commands, dependency pins, and validation limitations.

For a private CI evaluation build, open **Actions**, select a successful
**Windows builds and evaluation packages** run, download its **release** artifact,
extract it, and follow the included README. No development tools are required
to launch that package. Milestone 0.2 packages `audiohole.exe`, a native GUI
with deterministic simulated endpoints and routing behavior. No real audio is
processed, Test Beep emits no sound, and edits last only for the current session.
See [the GUI manual checklist](MANUAL_GUI_CHECKLIST.md) for evaluation workflows.
The presentation testing iteration adds editable [personality files](personalities/README.md),
a placebo DFA slider, scan callouts, clearer navigation, and Light/Dark/System palettes.

## Contributing

AudioHole is currently in early development. Contribution guidelines will be added as the project matures.

See our [AI Disclosure](AI_DISCLOSURE.md) for the official statement on AI assistance and human responsibility.

## License

A license will be selected before the project's public release.

---

*AudioHole: Because sometimes you just need to plug one thing into another.*
