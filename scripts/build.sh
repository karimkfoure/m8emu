#!/usr/bin/env bash
# Build one native product target with the official firmware as a local input.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
target=${1:-}
usage() {
  cat <<'HELP'
Usage: scripts/build.sh TARGET [-DM8_FIRMWARE=/path/to/M8_V6_5_2C_HEADLESS.hex] [CMake options...]
Targets: linux-x86_64, linux-arm64 (native or cross), macos (Apple Silicon).
First clone with --recurse-submodules, or run:
  git submodule update --init third_party/m8c

Required input: Headless firmware 6.5.2C, supplied by the user.
File: M8_V6_5_2C_HEADLESS.hex
SHA-256: 434dd3d2e06765b4c3c523f21142bc57cd0811b6a7dc6a7b9596876fcb744f5c
M8_FIRMWARE=/path/to/file.hex is also accepted as an environment variable.

All platforms: CMake >=3.21, Ninja, Python >=3.9 with venv/pip, SDL3.
The build can fetch pinned SDL3 and Python capstone if they are absent.
Use -DSDL3_DIR=/path/to/lib/cmake/SDL3 for an installed SDL3 package.
Linux x86_64: GCC, binutils, pkg-config.
Linux ARM64 native: the same tools on an ARM64 Linux host.
Linux ARM64 cross: aarch64-linux-gnu GCC/binutils, qemu-user, curl, dpkg-deb.
  Debian/Ubuntu packages: gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
  qemu-user curl dpkg-dev. The toolchain downloads SHA-256-pinned development
  packages for a glibc 2.28 sysroot. M8_ARM64_CC and M8_QEMU_AARCH64 select tools;
  M8_ARM64_SYSROOT selects an existing compatible ARM64 development sysroot.
  Native ARM64 builds use the same preset without the cross toolchain.
macOS: Xcode Command Line Tools (Apple Clang), CMake,
  Ninja, Python, pkgconf and SDL3. Install the tools before this command.
Fetched SDL3 also needs the platform's display and audio development headers.

Output: build/TARGET/ (including the executable and its adjacent dependencies).
M8_BUILD_DIR selects another output directory; M8_BUILD_JOBS selects jobs
(default 4; maximum 4 on Linux, 6 on macOS).
Release source exports disable PGO and BOLT.
HELP
}
case $target in
  linux-x86_64)
    preset=$(python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); print("linux-x86_64" if any(x["name"]=="linux-x86_64" for x in p["configurePresets"]) else "linux")' "$root/CMakePresets.json") ;;
  linux-arm64) preset=linux-arm64 ;;
  macos) preset=macos ;;
  -h|--help) usage; exit 0 ;;
  *) usage >&2; exit 2 ;;
esac
shift
jobs=${M8_BUILD_JOBS:-4}
limit=4
[[ $(uname -s) != Darwin ]] || limit=6
[[ $jobs =~ ^[1-9][0-9]*$ && $jobs -le $limit ]] || { echo "M8_BUILD_JOBS must be 1..$limit" >&2; exit 2; }
build=${M8_BUILD_DIR:-"$root/build/$target"}
configure=(--preset "$preset" -B "$build")
# Use the same ARM64 selections on an ARM64 host, without a cross toolchain.
if [[ $target == linux-arm64 && $(uname -s) == Linux && $(uname -m) =~ ^(aarch64|arm64)$ ]]; then
  configure+=(--toolchain '')
fi
[[ -z ${M8_FIRMWARE:-} ]] || configure+=("-DM8_FIRMWARE=$M8_FIRMWARE")
cd -- "$root"
cmake "${configure[@]}" "$@" "-DM8_PGO_JOBS=$jobs" "-DM8_PGO_TRAIN_JOBS=$jobs"
cmake --build "$build" --parallel "$jobs"
