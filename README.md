# m8emu

An unofficial, cycle-exact emulator of the Dirtywave M8 Headless for Linux and ARM64 handhelds, running the original 6.5.2C firmware.

m8emu runs the firmware's tracker, sequencer, synthesis engines and user interface without a Teensy. It uses an SDL3 window, the m8c renderer and a persistent SD card image. This is an early release. The Pi Zero 2 W can run simple projects, but complex projects can exceed its processing capacity.

```text
Original 6.5.2C firmware
        │
        ▼
AOT translation: Thumb-2/VFP → C
        │
        ▼
Native engine + exact device models
        │
        ▼
SDL3 host + m8c renderer → display, controls and audio
```

Ahead-of-time (AOT) translation converts firmware instructions to compiled host code. The original firmware remains the source of the instrument's behavior. The Unicorn CPU engine supplies reference execution and fallback. See [architecture](docs/architecture.md) for the execution and timing contracts.

## Build and run

Install GCC with matching `gcov` tools, Ninja, CMake 3.21 or newer, Python 3.9 or newer with virtual-environment support, binutils and `patchelf`. Then clone all submodules and build:

```sh
git clone --recursive https://github.com/karimkfoure/m8emu.git
cd m8emu
cmake --preset linux && cmake --build --preset linux
./build/linux/m8emu
```

The build verifies the pinned firmware, generates translated code, trains profile-guided optimization (PGO) on synthetic projects and compiles the final executable. It prepares the pinned Capstone Python package and fetches pinned SDL3 source if no installed SDL3 package is available. Initial setup needs network access. It needs no separate preparation command. Python is a build dependency; the application does not need Python at run time.

The `linux` and `linux-dev` presets select guarded native execution after reference boot. A generic x86-64 configuration keeps reference execution as its default. The desktop performance measurements below used that earlier reference-plus-AOT configuration.

To try a generated WAVSYNTH phrase without changing your card:

```sh
./build/linux/m8emu --demo simple --play
```

For development, use `cmake --preset linux-dev && cmake --build --preset linux-dev` to skip PGO. The experimental ARM64 preset is `cmake --preset handheld-arm64 && cmake --build --preset handheld-arm64`; it requires an AArch64 cross compiler and QEMU user emulator and can prepare its pinned older-glibc sysroot. The Pi measurements used a separately verified build recipe. Qualification of the public handheld preset is pending. See [building](docs/building.md). macOS, Android and web presets are planned.

## Status

“Tested” describes the stated host and test method. It does not certify every project or output device.

| Target | Audio | Display | Input | SD persistence | Host MIDI | Project SAVE/LOAD |
| --- | --- | --- | --- | --- | --- | --- |
| x86-64 Linux, AMD Ryzen 9 5900X | Tested: SDL3/PipeWire | Tested: SDL3/m8c | Tested: keyboard and scripted controls | Tested | Untested | Tested |
| ARM64 Linux, Pi Zero 2 W | Tested: simulated consumer; physical output untested | Tested: decoded firmware stream; physical display untested | Tested: scripted controls; physical gamepad untested | Tested | Untested | Tested: scripted session |
| R36S / Anbernic handhelds | Untested | Untested | Untested | Untested | Untested | Untested |
| macOS | Planned | Planned | Planned | Planned | Planned | Planned |
| Android | Planned | Planned | Planned | Planned | Planned | Planned |
| Web | Planned | Planned | Planned | Planned | Planned | Planned |

The core models USB MIDI input, output, clock and transport. These paths have reference comparisons on x86-64 and ARM64. The SDL host does not yet connect this API to external MIDI ports. Virtual gamepad checks pass; physical gamepad testing remains open.

## Performance

These are pre-release measurements of qualified builds. They are not measurements of a fresh v0.1.0 build with synthetic PGO. Compiler version, training inputs and project content affect speed. The public build trains only on generated projects; downloaded projects and samples are not included.

A rate of `1.0×` means one audio second per processing second. Unpaced benchmark rates use playback wall time. Paced application capacity excludes time spent waiting for the consumer. Capacity above `1.0×` alone does not prove continuous audio: a small ring can still empty during expensive sections.

| Reference project | Pi benchmark: AOT V2 | Pi later benchmark control | Pi paced application |
| --- | ---: | ---: | ---: |
| Idle, no sound | 1.144× | — | 1.159× |
| Simple WAVSYNTH phrase | 0.936× | 1.057× | 1.052× |
| Four-track drums | 0.698× | — | 0.657× |
| Eight-track heavy arrangement | 0.448× | 0.499× | — |

The Pi Zero 2 W measurements used its 1 GHz performance governor and matched reference PCM, the digital audio samples. AOT V2 rates cover six audio seconds with fresh AOT PGO. Later benchmark controls include ARM64 hardware floating point. Shared register state subsequently reached 1.143× on the simple benchmark and 0.530× on the heavy benchmark. These are separate build comparisons; their gains must not be added. The paced app used simulated audio. Its simple result uses the later shared-state and polling defaults; the four-track drum result is from an earlier build and had audio shortages.

On the AMD Ryzen 9 5900X running Linux, a 60-second simple-demo application run reached 4.203× capacity with AOT, versus 2.120× without AOT. Both simulated-consumer runs had zero underruns, missing frames, rejected frames or callback errors. A separate 12-second fullscreen run with PipeWire audio reached 3.950× capacity with zero audio errors. These desktop measurements use the reference engine with AOT, rather than the ARM64 native defaults.

### Real-project distribution

All 203 checked projects loaded in 6.5.2C and matched complete reference PCM and final schedules during their six-second playback windows on the Ryzen 9 5900X Linux host. Formats ranged from 1.3.1 to 6.1.1; tempos ranged from 20 to 360 BPM. Of these projects, 171 had eight static tracks, 38 lacked used sample dependencies, and seven had silent first windows. A successful LOAD does not establish that missing samples or the complete song are correct.

| Expanded native coverage measurement | x86-64 Linux: all 203 projects | Pi Zero 2 W: selected 19 projects |
| --- | ---: | ---: |
| Minimum capacity | 1.77× | 0.266× |
| 10th percentile | 3.30× | 0.308× |
| Median | 6.43× | 0.414× |
| 90th percentile | Not reported | 0.753× |
| Maximum | Not reported | 0.839× |
| Real time without audio shortages | Not measured by this sweep | 0 / 19 |

The desktop column divides audio duration by playback CPU time in concurrent correctness runs; it is not a dedicated wall-time benchmark. No desktop project had CPU capacity below `1.0×`. The Pi column uses one 30-second paced, simulated-consumer run per project at 1 GHz, with a 150 ms ring and 20 ms polling. The selected projects cover all five engines and a range of sample and effect use; projects with missing used samples or silent first windows were excluded. This is not a random sample or a whole-song test. These measured builds used project-based PGO before the public synthetic training recipe.

### Latency and boot

The desktop AOT note check measured 20.590 ms from input to simulated audio consumption with a 20 ms ring. This excludes audio-server, device and speaker delay. Linux ARM64 native builds use a 150 ms ring and 20 ms input polling. A protected simple-project run completed 60 seconds on the physical Pi with no simulated-consumer shortages and a maximum ring occupancy of 149.478 ms. The ring trades response time for tolerance of short processing bursts; it cannot repair sustained capacity below real time. Handheld speaker latency is unmeasured.

At 1 GHz on the Pi, reference boot took 4.755 seconds from reset to the checked post-boot entry. The optional native cold boot took 8.803 seconds at the same boundary. These times exclude process and library loading. Reference boot remains the default because native cold boot is slower and still requires Unicorn for runtime construction.

## Controls and storage

| Action | Default key |
| --- | --- |
| Move | Arrow keys |
| SHIFT | Left Shift or Z |
| PLAY | Space or X |
| OPTION | Left Alt or A |
| EDIT | Left Ctrl or S |
| Toggle note keyboard | Esc |
| Open settings | F1 |
| Mute host audio | F12 |
| Redraw display | R |

The gamepad D-pad moves the cursor. Default positional face buttons are west=SHIFT, north=PLAY, east=OPTION and south=EDIT. Printed button labels vary by controller. Hold Back/Select and Start together to exit. Existing m8c input settings take priority.

The default card is `$XDG_DATA_HOME/m8-live/card.img`, or `$HOME/.local/share/m8-live/card.img` if `XDG_DATA_HOME` is unset. The retained directory name preserves existing cards. A missing card becomes a 128 MiB FAT16 image. Use `--card PATH` to select a persistent image or `--image PATH` for a disposable copy. Clean exit replaces the persistent image atomically and keeps the previous image as `.bak`. Use Project SAVE or BUNDLE inside the firmware before exit; closing the application does not save unsaved tracker edits.

```sh
./build/linux/m8emu --card ./card.img --import ./content
./build/linux/m8emu --card ./card.img export ./exported
```

Import requires `--overwrite` to replace existing files. Export requires a new or empty directory. See [building and operation](docs/building.md) for read-only media, clean exit and headless checks.

## Limits

Only the pinned Headless 6.5.2C firmware is qualified. “Cycle-exact” refers to the emulator's guest-clock and event-order model. Reference comparisons check PCM, registers, memory, files and schedules for specified workloads; they do not establish independent physical Cortex-M7 timing fidelity. Complex music remains below real time on the Pi Zero 2 W. R36S performance, physical handheld audio/display, long thermal tests and external host MIDI remain untested. Project SAVE/LOAD is supported; emulator snapshots are not a user feature.

## Credits

- [Dirtywave](https://dirtywave.com/) created the M8. The build uses the pinned [M8 Headless Firmware](https://github.com/Dirtywave/M8HeadlessFirmware) submodule.
- [laamaa/m8c](https://github.com/laamaa/m8c) supplies the display renderer, input handling and configuration.
- [Unicorn](https://www.unicorn-engine.org/) supplies the reference CPU engine. This project uses the [maintained fork](https://github.com/karimkfoure/unicorn/tree/m8/2.1.4).
- [SDL](https://libsdl.org/) supplies portable window, input and audio services.

This project is unofficial and is not affiliated with Dirtywave. No project license has been selected. Third-party components retain their own terms.
