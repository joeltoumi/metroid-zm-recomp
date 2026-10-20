# Roadmap

## M1 — Native build ✅

- [x] The decompilation builds on x86-64 / ARM64 (`PLATFORM_PC`)
- [x] GBA memory map in host buffers
- [x] DMA (immediate, HBlank, VBlank), BIOS in C, frame loop with IRQs
- [x] Software PPU, headless host, smoke test in CI
- [x] Saving with the exact GBA layout (32 KiB, mGBA-compatible)

## M2 — Playable on PC and Android (in progress)

- [x] Game data loaded from the player's ROM (verified by SHA-1)
- [x] SDL3 frontend: scaling, fullscreen, keyboard, gamepad, touch,
      fast-forward, `settings.ini`
- [x] Android project + APK in CI
- [x] Validate with the real ROM (EU, headless): language, intro, title,
      file select, first Brinstar room and pause map
- [x] `MZM_WARP` test mode to load any room, cutscene or escape (see
      docs/PORT.md)
- [x] All 324 rooms, the 14 cutscenes, the escapes, the credits and the demos
      load and render correctly (EU ROM)
- [x] Out-of-bounds reads and writes caught by AddressSanitizer (rooms, map,
      intro, pause, Samus palettes, debris, Tourian escape): they now read
      from the ROM as on the GBA
- [ ] Full playthrough by hand (on a phone) to find what the automated tests
      do not see

## M3 — Sound ✅

- [x] Translate `asm/audio_internal.s` and `asm/soundcode.s` to C
- [x] Sound data (`sound/*.s`) read from the player's ROM
- [x] Emulate the PSG channels (1-4) and FIFO A/B; output via SDL_AudioStream
- [x] Compared against mGBA (intro and Brinstar, EU ROM)

## M4 — In-game options menu ✅

- [x] Overlay (ImGui) opened with Select+Start, Esc/F1, Back or a touch button
- [x] Control remapping from the menu (keyboard, gamepad, touch)
- [x] Video: integer scaling, filter, LCD/CRT effects, sync
- [x] Quick saves (save states, 4 slots with thumbnail) and configurable
      fast-forward (speed, hold/toggle)
- [x] Touch controls position/size/opacity editor
- [ ] Test the menu with gamepad and touch on a real device (AYN Thor)

## M5 — Enhancements

- [x] Widescreen (like Picori with Minish Cap): optional, off by default
- [x] 60 FPS logic / interpolation: sync with 60/120 Hz displays (sound
      resampled) and frame blending on 90/144 Hz (optional; real position
      interpolation was ruled out, see docs/PORT.md)
- [x] AYN Thor second screen: map or Samus status
- [x] Rumble
- [x] EU build (its own APK, CI with both regions)
- [x] JP build (its own APK and CI; not yet tested with the real Japanese ROM)

## Not supported

- The unlockable original Metroid (NES): it is an embedded GBA ARM emulator.
- Link cable connection with Metroid Fusion.
