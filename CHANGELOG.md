# Changelog

## 0.1.0

Initial public release of the original-firmware M8 Headless emulator.

- Add portable C11 machine and native execution support for the pinned 6.5.2C firmware.
- Add the SDL3 application with the m8c renderer, keyboard/gamepad controls and bounded stereo audio delivery.
- Add persistent FAT16 card images, clean-exit backups and file import/export.
- Add one-command Linux and experimental ARM64 build presets with generated AOT code and synthetic PGO training.
- Document measured desktop and Pi Zero 2 W capacity, audio-buffer limits and planned host ports.

Complex projects can exceed Pi Zero 2 W capacity. The earlier Pi build recipe was verified; qualification of the public handheld preset is pending. Physical handheld audio/display, external host MIDI, macOS, Android and web remain untested or planned. Earlier measured builds used project-based PGO; their speed is not a measurement of this release's synthetic training recipe.
