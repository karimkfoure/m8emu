# Building m8emu

m8emu builds from source on three targets:

| Target | Runs on |
|---|---|
| `macos` | Apple Silicon Macs |
| `linux-x86_64` | 64-bit Intel/AMD Linux |
| `linux-arm64` | 64-bit ARM Linux (Raspberry Pi) |

## 1. Get the firmware

You need the M8 Headless firmware **6.5.2C**, file `M8_V6_5_2C_HEADLESS.hex`.
The build checks its SHA-256 and refuses any other file:

```
434dd3d2e06765b4c3c523f21142bc57cd0811b6a7dc6a7b9596876fcb744f5c
```

## 2. Install the tools

**Linux (Debian/Ubuntu)**

```sh
sudo apt install build-essential cmake ninja-build python3-venv pkg-config
```

**macOS**

```sh
xcode-select --install
brew install cmake ninja python pkgconf sdl3
```

On Linux, if SDL3 is not installed, the build downloads and compiles it.
That step needs your desktop's development packages: X11 or Wayland,
plus ALSA or PulseAudio.

## 3. Build

```sh
git clone --recurse-submodules https://github.com/karimkfoure/m8emu.git
cd m8emu
export M8_FIRMWARE=/path/to/M8_V6_5_2C_HEADLESS.hex

scripts/build.sh macos   # or linux-x86_64, or linux-arm64
```

The first build takes a while, each build job uses about 4 GB of RAM. 
Set `M8_BUILD_JOBS=2` if you have little memory.

### Building for a Raspberry Pi or handheld

Small ARM boards don't have enough memory to build. Cross-build on a Linux
PC instead:

```sh
sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-user curl dpkg-dev
scripts/build.sh linux-arm64
```

Then copy the `build/linux-arm64/` folder to the device. On an ARM64 Linux
machine with enough memory, the same command builds natively.

## 4. Run

The build leaves everything in `build/<target>/`. Keep that folder
together: `m8emu` loads the firmware and libraries that sit next to it.

**Linux**

```sh
build/linux-x86_64/m8emu
```

**macOS**

```sh
build/macos/m8emu
```

You can also double-click `m8emu` in Finder.

**Raspberry Pi or handheld**

Copy the folder to the device and start it there:

```sh
scp -r build/linux-arm64 pi@raspberrypi.local:m8emu

# on the device
~/m8emu/m8emu                 # from a desktop
~/m8emu/m8emu --fullscreen    # from the console, without a desktop
```

### Using it

The M8 opens with a settings window beside it.

| Keys | Opens |
|---|---|
| Ctrl+, (⌘, on Mac) | Settings: keypad, theme, audio output |
| Ctrl+E (⌘E on Mac) | SD card explorer: add or export files |

Your SD card is a file at `~/.local/share/m8-live/card.img`. Pass
`--card PATH` to use another one. The emulator keeps your session when you
quit and resumes it next time. Save songs with PROJECT > SAVE, as on the
M8.

`m8emu --help` lists all options.
