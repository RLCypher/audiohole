# AudioHole

**Put your audio wherever you want.**

AudioHole is a free, open-source Windows audio routing application built around a simple idea: you paid for the hardware, so you ought to decide where the audio goes.

Got a headset that refuses to cooperate with your mixer? A collection of perfectly good audio devices that act like they've never met? Games, music, and voice chat all shouting over the same output like it's the last parking space at Costco?

Yeah. We got tired of that too.

## Download AudioHole

**Get AudioHole from GitHub Releases.**

Pick your preferred flavor:

- **[Windows installer](https://github.com/RLCypher/audiohole/releases/download/v0.1.0/AudioHole.Installer.v0.1-c2c6aca.exe):** For people who like their applications properly moved in, with their shoes by the door.
- **[Portable ZIP](https://github.com/RLCypher/audiohole/releases/download/v0.1.0/AudioHole.Portable.v0.1-c2c6aca.zip):** Unzip it into a folder and run it. No installation, no ceremony.

Both get you AudioHole. Pick whichever suits your particular brand of laziness. An interactive guided tutorial, a full user manual, and an advanced manual buffer tuning guide are provided to help you make sense of your new plumbing.

## A Quick Note About Security

AudioHole is currently distributed without a digital code signature. I don't intend to keep it that way forever. 
In the mean time, Microsoft Defender SmartScreen may throw a warning in your face because Windows doesn't recognize the publisher or hasn't established sufficient trust in the application. This warning doesn't necessarily mean AudioHole is malicious, but you should always verify where your software comes from.

SmartScreen is a legitimate Windows security feature, and I have no intention of asking you to disable it. I'm working toward getting AudioHole properly code-signed so Windows can recognize its publisher. Until then, Windows may remain understandably suspicious of my holes.

AudioHole does not require an internet connection to do its job. Diagnostic logging, when enabled, stays on your computer. Nothing gets automatically uploaded, those logs only go somewhere else if you decide to Ctrl-C, Ctrl-V them into the wild.

The only time AudioHole reaches out to the internet is when you enable "Automatically check for updates", and even then, no more than once a week, and only back to the AudioHole GitHub to compare version numbers. I won't stand for wasting your precious bandwidth on telemetry data.

## What does it do?

### AudioHole

**Device-agnostic audio forwarding.** Connect Windows capture and playback devices without worrying about whose logo is on the box. Your microphone doesn't need to know who manufactured your mixer, and frankly, neither should you.

Set up a connection, tell the audio where to go, and let the software handle the introductions.

### Plug Board

**Application audio routing.** Give your apps somewhere to put their noise. Organize games, music, voice chat, and other sound-making troublemakers into sockets, then point those sockets at the playback devices you want.

It's cable management for your applications, except you don't have to crawl behind a desk or find a flashlight with working batteries.

## Getting started

Run AudioHole and follow the interactive tutorial if you'd like someone to show you where everything goes. If you're the type who starts turning knobs before reading labels, there's also a full user manual and a buffer tuning guide for when you decide to get particularly intimate with your audio settings.

You may also encounter **Norm and Horu**, AudioHole's resident personalities. They offer helpful advice, unsolicited commentary, and varying degrees of professionalism. They're optional. Their opinions about that are their own.

## Requirements

- Windows 10 or Windows 11
- 64-bit (x64) computer
- Compatible Windows audio devices

AudioHole works with existing Windows audio endpoints. It does not install a custom virtual audio driver. Your existing devices provide the holes. We just help connect them.

## Documentation and assorted reading material

The download includes the guided tutorial, full user manual, and manual buffer tuning guide.

Curious why someone spent this much effort making Windows audio behave? Read the manifesto. Want to know how much AI helped build this contraption? There's an AI disclosure for that too.

## License

AudioHole's software is released under the **GNU General Public License, version 3 (GPLv3)**. The full legal text is in LICENSE.

Original mascots, logos, and certain other creative assets have separate terms described in `licenses/assets.txt`. Third-party components retain their applicable notices and licenses.

---

**Plug it in. Route it. Get on with your life.**
