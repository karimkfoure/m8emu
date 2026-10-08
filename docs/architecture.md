# Architecture

m8emu executes the original M8 Headless firmware. It accelerates guest execution without replacing the tracker, sequencer or synthesis algorithms. The portable machine, generated execution code and host services have separate responsibilities so that a new host can retain the same musical behavior.

```text
Firmware bytes ──► AOT translator ──► compiled execution libraries
      │                                      │
      ▼                                      ▼
Machine state ◄──── reference/native execution owner
      │
      ├── ordered display bytes ──► m8c decoder and SDL renderer
      ├── final-producer PCM ─────► bounded audio ring ──► SDL audio
      ├── SDHC operations ────────► private FAT card image ──► persistence
      └── controller packets ◄──── ordered input queue ◄──── SDL input
```

## Source boundaries

| Location | Responsibility |
| --- | --- |
| `core/include/m8emu/` | Public machine API |
| `core/machine/` | Guest memory, time, interrupts and peripheral models |
| `core/engine/` | Native dispatch, compiled regions, instruction handlers and fallback |
| `core/storage/` | Card creation, FAT import/export, publication and backups |
| `core/platform/` | Host clocks, threads, memory reservation and floating-point state |
| `aot/` | Thumb-2/VFP translation, entry catalogs and code generators |
| `pgo/` | Synthetic project generation and build-time training |
| `hosts/sdl/` | Application lifecycle, m8c integration, input, display and audio |
| `third_party/` | Pinned firmware, Unicorn fork and m8c submodules |

## Execution and time

The machine models NVIC interrupts, SysTick, DWT, SAI/eDMA audio, USB, SDHC storage, SNVS RTC and the watchdog. Guest clocks and event ordering belong to the machine, not the window loop or audio device. Host deadlines bound a service slice; the execution owner yields after completed effects and preserves the next resume point.

The reference engine uses Unicorn with optional translated code. The native engine dispatches translated functions, larger compiled regions and exact instruction handlers. Generated entries check firmware code bytes and memory assumptions. A declined native operation returns before its effects; a completed operation resumes after those effects. This rule prevents duplicated writes or interrupt service during fallback. Unsupported execution and unsafe state produce explicit diagnostics.

The public `linux`, `linux-dev` and `handheld-arm64` presets select reference boot, then install warm native execution at a checked post-boot boundary. Generic x86-64 configuration keeps reference execution with AOT as its default. Historical desktop measurements used that reference configuration. The earlier ARM64 recipe was verified on the Pi Zero 2 W; qualification of the public handheld preset is pending.

The optional native cold owner executes reset and boot through instruction handlers, then installs warm dispatch without transferring CPU state. Its matching cold libraries are not included by the default presets. It still requires Unicorn during runtime construction. Native cold boot is not a Unicorn-free executable.

“Cycle-exact” describes modeled guest clocks, instruction effects and device-event ordering. The checks compare these results with reference execution for declared inputs. They do not establish independent physical Cortex-M7 timing or complete system-on-chip fidelity.

## Host ownership and queues

One guest thread executes firmware and reads mutable machine state. The main thread handles SDL input and rendering. Controller packets keep press/release order and enter as complete packets. A full input queue reports an error rather than dropping a musical action.

Display updates use a separate bounded queue. If rendering stops long enough to fill it, the host discards stale display deltas, resets the decoder and requests a complete redraw. This changes display maintenance traffic under pressure while preserving music and ordered input. The guest does not wait for the renderer.

The firmware's local final-producer tap supplies 44,100-frame-per-second stereo S16LE PCM. A bounded single-producer/single-consumer ring separates production from delivery. The SDL callback only consumes completed PCM. It performs no guest execution, storage access or diagnostic file writing. The original USB audio stream is drained separately.

The core MIDI API preserves complete USB MIDI messages and guest timestamps. External MIDI-device routing is a host responsibility and is not exposed by the current SDL application.

## Persistence

The firmware sees an image-backed SD card and performs its own filesystem operations. The host opens a private writable copy, holds an exclusive lock and checks publication conditions at exit. It writes the prior image as `.bak` and replaces the card atomically. Firmware and card paths must not alias. Errors and incomplete writes prevent unsafe publication.

Card publication preserves firmware file writes. It does not convert unsaved tracker RAM into a saved project. User-facing SAVE/LOAD means the firmware's project operations; whole-emulator snapshots are not an application feature.

## Correctness and performance

Reference comparisons cover complete PCM, decoded and raw display output, saved files, guest schedules, registers and memory for declared workloads. A complete LOAD/PLAY/edit/STOP/keyjazz/SAVE session matched on x86-64, ARM64 under QEMU and the physical Pi Zero 2 W, including 436 checkpoints of 58 register values. Broader post-boot checks cover MIDI and all instrument engines. This is finite workload coverage, not proof for every possible project.

Performance checks use a separate run because tracing and counting add host work. Unpaced playback benchmarks, paced application capacity and audio continuity answer different questions. CPU-time capacity from concurrent runs is not dedicated wall-time speed. A headless simulated-consumer pass does not test physical audio or rendering. The [README](../README.md#performance) states the hardware, window length and limits of the reported measurements.

## Build-time optimization

AOT output is generated from the pinned firmware into the build directory. Profile-guided optimization first collects compiler profiles from generated synthetic projects, then uses those profiles in the final build. Training projects contain no downloaded music or samples. Profiles depend on the generated source, compiler version, target and training recipe; incompatible profiles must be rebuilt.

Earlier performance measurements used several qualified project-based PGO recipes. Those historical rates do not establish the speed of the public synthetic-trained release. Reference PCM equality remains a separate requirement from an optimization's speed gain.
