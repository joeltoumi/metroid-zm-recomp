# mzm-android

Native port of **Metroid: Zero Mission** (GBA) to Android and PC, built on top
of the [metroidret/mzm](https://github.com/metroidret/mzm) decompilation.

It is not an emulator: the game's C code is compiled directly for ARM64 /
x86-64, and the "console" (video, DMA, interrupts, BIOS) is provided by a thin
layer in `port/`.

> **Status: early development.** The game builds, boots and plays sound
> natively; a full playthrough with the real ROM still has to be validated.
> See [docs/ROADMAP.md](docs/ROADMAP.md).

## You need your own ROM

The repository and the APKs **do not contain any game data**. On first launch
the app asks for your dump of *Metroid - Zero Mission*. There is one build per
region, and each one only accepts its own ROM:

| Region | Build | SHA-1 |
| --- | --- | --- |
| Europe | `eu` | `0fd107445a42e6f3a3e5ce8c865f412583179903` |
| USA | `us` | `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8` |
| Japan | `jp` | `096f07685a3dc9286e71aa0b761f233b5efa2fcd` |

The ROM is copied to the app's private storage, and the graphics, levels and
text are loaded from it on every launch. The European version starts with the
language selection, as on the GBA. The Japanese build compiles and passes the
smoke test, but has not been tested with the real ROM yet (see docs/PORT.md).

## Installing on Android

Every push to `main` produces three APKs under **Actions → build → Artifacts →
`mzm-android-apk`** (arm64 + x86_64): `app-eu-release.apk` for the European
ROM, `app-us-release.apk` for the American one and `app-jp-release.apk` for the
Japanese one. They can be installed side by side. Install the one for your
region, open *Zero Mission (EU)*, *(US)* or *(JP)* and pick your `.gba` with
the file picker.

To build just one: `./gradlew assembleEuRelease`, `assembleUsRelease` or
`assembleJpRelease`.

## Building on PC (Linux)

Requirements: CMake ≥ 3.22, Ninja, Python 3, a C11 compiler and SDL3 (if it is
not installed, CMake downloads and builds it).

```sh
cmake -B build -G Ninja                 # USA ROM
cmake -B build -G Ninja -DMZM_REGION=eu # European ROM
cmake -B build -G Ninja -DMZM_REGION=jp # Japanese ROM
cmake --build build
./build/mzm path/to/your_rom.gba
```

On PC all builds share the same data folder, so each region keeps its save and
its copy of the ROM separately (`mzm_eu.sav`, `rom_eu.gba`, `mzm_jp.sav`...).

Without a window (tests/CI): `-DMZM_HOST=headless`, which dumps frames to PPM:

```sh
./build/mzm_headless --rom mzm_us_baserom.gba --frames 1200 --dump-every 60 \
    --press 400:S:5      # press Start at frame 400 for 5 frames
```

`--state-save FRAME:FILE` and `--state-load FRAME:FILE` save or load a state at
that frame (for reproducible tests). `--width 284` renders in widescreen
(16:9).

## Controls and settings

The options menu pauses the game and opens with **Esc** or **F1**, with
**Select+Start** (or the Guide button) on a gamepad, with the Android **Back**
button, or with the `=` button at the top of the touch controls. From there:

- **Quick saves**: 4 slots with thumbnail and date. `F5` saves and `F8` loads
  the last slot used. They do not touch the normal save (`.sav`).
- **Controls**: rebind any keyboard or gamepad button (tap the box, then the
  new key/button) and the stick dead zone.
- **Video**: fullscreen, integer scaling, smooth filter, LCD effect (pixel
  grid) or CRT (scanlines), VSync and **widescreen** (16:10, 16:9, 2:1 or
  match the display; off by default): you see more of the room on the sides
  instead of black bars, without changing the game. Menus, map and cutscenes
  stay at 3:2 in the center.
- **Smoothness** (under Video): *match speed to display* removes stutter on
  60/120 Hz displays (the game runs 0.46% faster, sound adjusts
  automatically) and *frame blending* smooths motion on 90/144 Hz displays.
  Both are off by default; see
  [docs/PORT.md](docs/PORT.md#smoothness-60120-hz-and-frame-blending).
- **Sound and speed**: volume, fast-forward speed (x2-x10) and whether its
  button is held or toggles.
- **Touch controls**: when they are shown, opacity, and an editor to move
  each button, change its size and which GBA button it presses.
- **Map and rumble**: gamepad and device rumble (with its strength) and what
  the second screen shows: the area map with Samus's position, or her status
  (energy, tanks, ammo and equipment). On the AYN Thor it goes on the bottom
  screen; on PC it opens a second window.

Settings are stored in `settings.ini`, in the app's data folder
(Linux: `~/.local/share/mzm-android/mzm/`), which can also be edited by hand.
It is created with the default values the first time.

| GBA | Keyboard | Gamepad |
| --- | --- | --- |
| A / B | X / Z | right / bottom face button |
| L / R | A / S | bumpers or triggers |
| Start / Select | Enter / Backspace | Start / Back |
| D-pad | arrow keys | D-pad or left stick |
| Fast-forward | Tab (hold) | right stick click |

`F11` toggles fullscreen. On touchscreens a virtual gamepad appears
(`touch_controls = auto | on | off`) that hides when a physical controller is
used, such as the AYN Thor's built-in controls. Select+Start opens the menu;
the game's reset (A+B+Select+Start) still works if A and B are pressed first.

Saves are stored in `mzm.sav` with the same format as the GBA, so an mGBA
`.sav` of the USA version works as-is.

## How it is built

- `src/`, `include/`: the decompilation. Changes for the native build are
  wrapped in `#ifdef PLATFORM_PC` and do not alter the GBA ROM.
- `port/src/`: emulated memory, DMA, BIOS in C, frame loop with
  VBlank/HBlank/VCount IRQs, ROM loading, save states and frontends (SDL3 with
  the menu in [Dear ImGui](https://github.com/ocornut/imgui), and headless).
- `port/ppu/`: software PPU from [Project Picori](https://github.com/999sian/tmc)
  (the native port of Minish Cap).
- `tools/port/gen_assets.py`: compiles the data in `src/data` zero-filled and
  generates the registry that fills it from the ROM at startup.
- `android/`: Gradle project (SDL3 + `libmain.so`).

Details in [docs/PORT.md](docs/PORT.md).

## Licenses

The decompilation is MIT ([LICENSE](LICENSE)); the port layer is GPL-3.0
([port/LICENSE](port/LICENSE)). Metroid is property of Nintendo; this project
does not include or distribute any of its property.

The decompilation's original README is in
[docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).
