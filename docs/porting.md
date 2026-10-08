# Porting

A port must keep the firmware's musical and file behavior while adapting the host services. The core is portable C11, but a compiled library is specific to its CPU, compiler and firmware version. A successful cross-build does not establish target audio, display or thermal performance.

## Current targets

| Target | Current state | Work still required |
| --- | --- | --- |
| x86-64 Linux | SDL3 application tested | Broader physical input and audio-device coverage; external MIDI routing |
| ARM64 Linux, Pi Zero 2 W | Earlier recipe tested for execution, scripted controls, persistence and simulated audio | Public handheld preset qualification; physical audio/display checks and broader continuous-playback performance |
| R36S / Anbernic | Experimental ARM64 preset; hardware untested | Preset qualification, driver, gamepad, memory, audio latency and sustained thermal checks |
| macOS | Planned | Platform validation, SDL integration and target PGO |
| Android | Planned | NDK toolchain, lifecycle, low-latency audio, storage and input host |
| Web | Planned | Emscripten build, native-only construction, floating-point handling, browser audio and storage |

## Preserve the machine contract

Keep guest time separate from host deadlines. Preserve instruction effects, interrupt order, partial effects on faults and exact resume points. A fallback must not replay an operation that already completed. Validate firmware bytes and compiled-code compatibility before execution.

Implement host clocks, threads, memory reservation, thread-local storage and floating-point control in `core/platform/`. Keep operating-system calls out of the device models and translator. Some native execution paths reserve memory at fixed guest-address offsets; an incompatible address layout must reject that path before access.

ARM64 acceleration controls the host floating-point environment to reproduce guest behavior, including flush-to-zero and exception flags. Restore the host thread's floating-point state at the boundary. Verify numerical behavior on the target CPU; an x86 pass does not establish ARM behavior.

## Supply host services

- Run firmware on one owned execution thread. Send whole ordered controller packets, including releases on focus loss, disconnect and close.
- Decode the firmware's display stream. Preserve decoder identity and request a complete redraw after queue recovery.
- Deliver stereo S16LE audio at 44,100 frames per second through a bounded queue. Keep firmware execution and file access out of the audio callback. Measure actual device request sizes, shortages and delivered sample-clock rate.
- Provide private card storage, exclusive access, atomic replacement and backups. Preserve read-only behavior and report failed writes. Adapt the storage provider rather than bypassing firmware SAVE.
- Route MIDI messages with explicit order and timing if the host exposes MIDI. Core API tests do not qualify external devices or hotplug.

Choose queue sizes from measured target behavior. The current ARM64 native policy uses 150 ms audio buffering and 20 ms input polling. This reduced shortages for a protected simple Pi project; it does not establish speaker latency or real-time capacity for all music.

## Planned hosts

### macOS

SDL3 supplies the main host interfaces, but the platform memory, file-publication and floating-point paths still need target checks. Both x86-64 and Apple Silicon need their own compiler profiles and measured audio latency. The preset is planned; Linux results do not establish a macOS pass.

### Android

Use the NDK for the ARM64 core and retain the guest-memory and floating-point contracts. The Android host must manage activity pause/resume, audio focus, output-device changes, storage access and control releases. Measure callback behavior and sustained thermal capacity on a named device. PGO training belongs to the target toolchain; Linux ARM64 profiles are not portable to an NDK build.

### Web

Use Emscripten for generated native execution. Unicorn's just-in-time engine is not a practical browser execution path. The runtime still needs construction without the Unicorn library before this is a complete port. WebAssembly cannot set the ARM flush-to-zero control; generated code needs an equivalent software path. Browser audio startup, worklet delivery, input focus and durable storage also require host implementations and checks.

## Validate on the target

Start with reset to the expected SONG display, ordered input and a generated simple project. Check LOAD, audible playback, pitch, note release, STOP and silent output. Then save and reopen in a fresh process, including sample dependencies and failed writes. Compare complete PCM and schedules against the reference for deterministic workloads.

Measure physical audio separately from a simulated consumer. Report the device, compiler, firmware, clock policy, window length, ring size and shortage counts. Test concurrent input, rendering and storage during playback. A short clean project does not establish whole-song coverage, long thermal behavior or the capacity of a different handheld.
