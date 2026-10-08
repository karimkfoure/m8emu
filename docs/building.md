# Building and operation

The `linux` preset builds the application, translated firmware code and profile-guided optimization (PGO) in one command. A fresh recursive clone supplies the pinned firmware and source dependencies; no downloaded music or samples are required.

## Linux

Install Git, GCC with matching `gcov`, `gcov-dump` and `gcov-tool`, Ninja, CMake 3.21 or newer, Python 3.9 or newer, binutils (`readelf`) and `patchelf`. Python needs `venv` and `pip`, or `uv` as the virtual-environment fallback. Keep the compiler and profiling tools from the same toolchain: profile data is not portable between compiler versions.

CMake uses an interpreter with `capstone==5.0.7`, or creates one under the build directory and installs that pinned package. If no SDL3 package is installed, CMake fetches and builds pinned SDL3 source. Initial setup therefore needs network access. Install the graphics and audio development dependencies for the SDL drivers you need. To select an existing SDL3 installation, configure with `-DSDL3_DIR=PATH`.

```sh
git clone --recursive https://github.com/karimkfoure/m8emu.git
cd m8emu
cmake --preset linux && cmake --build --preset linux
./build/linux/m8emu
```

If you cloned without submodules, run `git submodule update --init --recursive` first. The build verifies `third_party/M8HeadlessFirmware/Releases/M8_V6_5_2C_HEADLESS.hex` before translation. A firmware verification failure is a wrong input; use the pinned submodule revision rather than disabling the check.

PGO builds instrumented libraries, runs generated synthetic training projects and builds the final profile-use application. It rejects missing or incompatible profile objects and compares complete PCM and final schedules between the instrumented and final builds. Generated C, profiles and training media belong in the build directory. The application does not need Python or training media at run time.

The `linux` and `linux-dev` presets both enable guarded native execution after reference boot, including on x86-64. Generic x86-64 configuration still defaults to reference execution. Thus the older desktop reference-plus-AOT rates in the README describe a different execution policy from the public presets.

Use the development preset to skip PGO; it also permits a non-GCC C compiler:

```sh
cmake --preset linux-dev && cmake --build --preset linux-dev
./build/linux-dev/m8emu --demo simple --play
```

## ARM64 handheld bundle

The `handheld-arm64` preset cross-compiles on a Linux build host. The earlier build recipe was verified on the Pi Zero 2 W; qualification of this public preset is pending. Do not treat the historical Pi results as a pass for this preset.

Install `aarch64-linux-gnu-gcc`, its matching GCC profiling tools and cross binutils, `qemu-aarch64` or `qemu-aarch64-static`, `curl` and `dpkg-deb`, in addition to the Linux build tools above. The toolchain at `cmake/toolchains/handheld-arm64.cmake` prepares a pinned glibc 2.28 sysroot under `build/toolchains/handheld/` and isolates the compiler from newer host headers and startup files. It uses QEMU with that sysroot for PGO training. Use a fresh build directory when changing the compiler, sysroot or execution options.

```sh
cmake --preset handheld-arm64 && cmake --build --preset handheld-arm64
```

Optional environment variables are `M8_ARM64_CC` for the cross compiler, `M8_ARM64_SYSROOT` for an existing compatible development sysroot and `M8_QEMU_AARCH64` for the QEMU executable. `M8_ARM64_TOOLCHAIN_CACHE` is a CMake cache path for the prepared toolchain. The default paths require no overrides.

The output is `build/handheld-arm64/m8emu` with `lib/`, `firmware/` and `gamecontrollerdb.txt` beside it. Keep these together when copying the bundle. The build rejects bundled ELF files that require GLIBC newer than 2.27. Target graphics drivers remain system dependencies. Physical R36S and Anbernic audio, display and controls remain untested.

For a handheld display, run the bundled executable with `--fullscreen --fps 30 --integer-scale`. SDL chooses the installed video and audio drivers. For example, `SDL_VIDEODRIVER=kmsdrm` selects KMSDRM when SDL provides that driver. It requires a local console session and DRM-device access. The existence of this driver option does not establish hardware testing.

macOS, Android and web builds are planned. Their host requirements are described in [porting](porting.md).

## Storage

The default card is `$XDG_DATA_HOME/m8-live/card.img`, or `$HOME/.local/share/m8-live/card.img` if `XDG_DATA_HOME` is unset. The retained directory name preserves existing cards. A missing card starts as a 128 MiB FAT16 image. The firmware creates its initial content. A 128 MiB image uses more memory than the 16 MiB generated demo, and import can temporarily hold multiple copies.

Use `--card PATH` for a persistent image. The application locks it for exclusive use, works on a private copy and publishes safe firmware writes at clean exit. Publication keeps the prior image as `.bak` and atomically replaces the card. `--read-only` refuses writes and prevents card publication. `--image PATH` loads a disposable copy. `--demo simple` creates an in-memory demo without changing the persistent card.

Save the project with the firmware's SAVE or BUNDLE operation before exit. Publishing the card writes files that the firmware has saved; it does not save unsaved tracker state. Close the window, use Alt+F4 or send SIGINT/SIGTERM for a clean exit. Runtime errors, incomplete writes and read-only media block unsafe card publication.

```sh
./build/linux/m8emu --card ./card.img --import ./content
./build/linux/m8emu --card ./card.img --import ./content --overwrite
./build/linux/m8emu --card ./card.img export ./exported
```

Import runs before boot and becomes persistent after clean exit. It refuses links, special files, invalid names and file/directory conflicts. Existing files require `--overwrite`. Export runs without booting and requires a new or empty directory. Both operations preserve nested directories and file bytes. Import can change FAT allocation and timestamps; a failed host export can leave partial files in its destination.

## Audio and diagnostics

The firmware produces stereo signed 16-bit PCM at 44,100 frames per second. The SDL audio callback consumes a bounded ring; it does not execute firmware or read card files. F12 mutes output while consumption continues, so the guest clock still advances.

Linux ARM64 native builds use a 150 ms ring and 20 ms input polling. Other builds use a 20 ms ring and 1 ms polling. `--queue-ms N` and `--poll-ms N` override these values. The audio device adds its own buffering. A larger ring can cover short bursts, but it cannot make a slow project run at real time.

Run a short generated-demo check without a window or audio device:

```sh
./build/linux/m8emu --headless --demo simple --seconds 6
```

The headless mode uses a simulated audio consumer by default. It checks playback and audio delivery; it does not test physical speakers. To use SDL audio without a window, add `--consumer sdl`. `--latency-test --headless --demo simple` checks a note's pitch, onset and release at the simulated consumer; it excludes audio-server and speaker latency.

Use `--no-native-engine --no-native-boot` to select reference execution. The public presets normally boot through the reference engine and then use guarded native playback. Native cold boot needs matching cold libraries, which the default presets do not include; its runtime selector is `--native-boot`. It still links Unicorn and is slower in the measured Pi startup check. `--help` lists all command options.
