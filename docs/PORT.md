# Port technical notes

## How it boots

1. `port/src/main.c` (`Port_Main`) replaces `asm/crt0.s`: it starts the host,
   loads the ROM, resets the "console" and calls `agbmain()` in a loop (the
   game returns from `agbmain` on a *soft reset*).
2. The game loop ends each frame with `SYSCALL(2)` (Halt). Natively that is
   `Port_RunFrame()` (`port/src/frame.c`), which reproduces one hardware
   frame: VBlank (VBlank DMA + the game's VBlank IRQ), the 160 visible lines
   with their HBlank/VCount events, the PPU render, presenting, reading input
   and waiting for the next tick (59.73 Hz).

## Memory

`include/gba/memory.h` redefines `EWRAM_BASE`, `REG_BASE`, `VRAM_BASE`... under
`PLATFORM_PC` so they point to host buffers (`gPortEwram`, `gPortIo`...). The
game code does not change: it writes to registers as always and the PPU reads
those same buffers. The symbols that `linker.ld` places in EWRAM at fixed
addresses become macros over `gPortEwram` + offset, so they still overlap as on
the GBA.

Whatever does not fit in a 32-bit register (64-bit pointers in DMA) goes
through `Port_DmaSet` instead of writing the DMA registers.

Busy-waits on `REG_VCOUNT` advance a virtual counter (`READ_16` in
`include/io.h`).

## Game data

`tools/port/gen_assets.py` (run by CMake):

- writes zero-filled `.inc` stubs with the exact element count from
  `database.json`, so every array has its real size;
- preprocesses each `src/data/**.c` to see which `#include "extracted/..."`
  are active for the chosen region, and generates a wrapper that includes the
  original and registers (array address, ROM offset, size) in a constructor;
- the wrappers are compiled with `-Dconst=` so the arrays are writable (almost
  all of them end up in `.bss`).

At startup, `port/src/assets.c` verifies the ROM's SHA-1 and copies each
block. `MZM_DEV_SKIP_ROM_CHECK=1` allows booting with a fake image (only for
the CI smoke test).

### Reads past the end of an array

There are places where the game reads more bytes than the array holds
(fixed-size copies, indices that overshoot: the map, the collision tables, the
common palette, the intro, the Tourian escape...). On the GBA that reads the
data that follows in the ROM; natively each array is a separate variable. To
read the same thing:

- `ROM_DATA(ptr)` (`include/macros.h`) returns the same data inside the ROM
  copy, using the asset table (`Port_RomMirror`).
- `ROM_DATA_ARRAY(array)` does the same for arrays that are compiled into the
  game instead of extracted: it locates them in the ROM by their contents
  (`Port_RomFind`, which requires them to appear exactly once).

When the extra data read is never used (the collision tables, Samus palettes
outside her pose, a debris particle with no free slot) the read is trimmed or
the write is discarded.

## Sound

The MZM engine is almost entirely C (`src/audio.c`, `src/audio_wrappers.c`,
`src/music_wrappers.c`); its internal routines and the mixing loops were in
ARM assembly and are translated in `port/game/audio_engine.c`, keeping the
exact arithmetic.

The tracks, voicegroups and samples are assembly data with 32-bit pointers
between them, so they are not copied: the player's ROM stays in memory and the
engine reads the scripts and samples in place (`Port_RomPointer`). Only what
changes shape on 64-bit is rebuilt:

- `sSoundDataEntries` (pointers to the track headers). Its address changes by
  region, so it is located in the ROM: it is the only run of 708 consecutive
  valid entries.
- The voicegroups (`struct Voice` is 12 bytes on the GBA and 24 natively).
- The pointers inside the scripts (track list, GOTO/PATT) are translated as
  they are read.

The hardware is emulated by `port/src/apu.c`: each frame it plays the samples
that DMA 1/2 would feed to FIFO A/B (raising the DMA 2 IRQ every 16, like the
console) and the four PSG channels from their registers. Output is 48 kHz
stereo via `SDL_AudioStream`; the headless host can record it with `--wav`.

It has been compared against mGBA by recording the same thing on both (intro
and Brinstar with the European ROM): the waveform matches (correlation
0.98-0.99 on the Brinstar music) and so does the volume.

## Save states

`port/src/savestate.c`. The machine state is the GBA memory (EWRAM, IWRAM,
I/O, palettes, VRAM, OAM; not SRAM, so the real save is not touched) plus the
`mzm_state` section: under `PLATFORM_PC`, `IWRAM_DATA` puts all the game's
global variables there, and `PORT_STATE` the port's own variables that are
part of the "hardware" (DMA channels, sound chip).

Saving and loading only happen at the `Halt` at the end of the `agbmain` loop
(the game's only `SWI 2`; the host receives `Port_HostFrameBoundary`), where
the C stack is always the same and does not need to be saved. That is why the
menu runs there, with the game paused.

Natively many variables are pointers (to the executable's code and data, or to
the player's ROM, which the sound engine reads in place), and those addresses
change on every run (ASLR, `malloc`). The state records where they were and,
on load, slides every 64-bit value that falls into one of those ranges. A
state from another build (another app version) is rejected. The converted
voicegroups are rebuilt in the same order from their addresses.

Tested with `mzm_headless`: a state saved in one process and loaded in another
produces exactly the same frames, and the two states differ only in the
relocated pointers.

## Test mode (warp)

To test areas without playing up to them, the `MZM_WARP` environment variable
starts a new game directly in a room, with all items (`port/src/warp.c` and
`port/game/warp.c`):

```sh
MZM_WARP=kraid:3 ./mzm                      # one room
MZM_WARP=norfair:* MZM_WARP_FRAMES=150 \
  ./mzm_headless --dump-every 150 ...        # every room in an area
MZM_WARP=cutscene:* MZM_WARP_FRAMES=1500 ...  # the 14 cutscenes
MZM_WARP=tourianescape:0                     # also chozodiaescape, credits
```

Areas are given by name (`brinstar`, `kraid`, `norfair`, `ridley`,
`tourian`, `crateria`, `chozodia`) or number. With several rooms, each one
lasts `MZM_WARP_FRAMES` frames (300 by default).

With this, all 324 rooms of the 7 areas, the 14 cutscenes, both escapes, the
credits and the title screen demos have been loaded under AddressSanitizer
with no errors.

## Rumble and second screen

`port/game/status.c` reads, once per frame (from `Port_RunFrame`, before
presenting), the game's global variables: Samus's energy, ammo, equipment,
pose and position, bombs and power bombs, screen shake and the area map that
the game keeps for the minimap. It only reads; the game code does not change.
By comparing with the previous frame it detects events: damage (energy lost +
invincibility frames), continuous drain (lava, acid, metroids), a bomb or
power bomb exploding, a fall of more than 7 blocks, a shinespark crash,
Samus's death and an item pickup. Screen shake (bosses, escape earthquakes)
rumbles while it lasts. That copy of the previous frame is not machine state:
it is forgotten when a save state is loaded so no false events are seen.

- **Rumble** (`port/src/extras.c`): each event triggers a short effect; the
  strongest one drives both gamepad motors (`SDL_RumbleGamepad`). The device
  vibrator (on Android, SDL's `VIBRATOR_SERVICE` haptic device) only supports
  pulses, so it gets one each time the rumble starts or increases.
  `MZM_RUMBLE_LOG=1` writes each event to the log (also with `mzm_headless`,
  to test it with `MZM_WARP` and `--press`).
- **Second screen** (`port/src/second_screen.c`): drawn in software on a
  canvas 256 pixels wide. The map uses the pause map's tiles and palettes with
  the explored/downloaded rooms (`gDecompressedMinimapVisitedTiles`), framed
  on the area's rooms, and Samus's tile blinks; the status is text in an 8x8
  font. On PC it is a second SDL window (on another monitor if there is one;
  closing it turns it off). On Android SDL only has one window, so
  `MzmActivity` looks for a secondary display (`DisplayManager`,
  *presentation* category first), shows a `Presentation` there that never
  takes focus, and fetches the image via JNI (`host_sdl.c`) on every refresh.
  Without a second screen it does nothing. `mzm_headless --screen2 map|status`
  also dumps that image with each capture.

Options in `settings.ini` (`[rumble] gamepad, device, strength` and
`[second_screen] mode = off | map | status`) and in the menu. Rumble is off by
default; the second screen shows the map on Android (it only does something if
there is a second display) and is off on PC.

## Smoothness: 60/120 Hz and frame blending

The GBA draws 59.7275 frames per second and almost all displays run at 60 Hz
(or 90, 120, 144...). By default the game follows the GBA clock
(`Port_HostWaitFrame`), so on a 60 Hz display with VSync one frame is shown
twice every ~3.7 s (a stutter; without VSync, the image tears), and at 144 Hz
frames last 2 or 3 refreshes irregularly. There are two options in
`settings.ini` (`[video]`) and in the menu (Video → Smoothness), both off by
default and only in the SDL frontend (`port/src/host_sdl.c` +
`port/src/pacing.c`):

- `sync_to_display = true` (*Match speed to display*): if the display refresh
  rate is a multiple of ~59.73 Hz within a 1% margin (59.94, 60, 120, 180,
  240 Hz...), the game runs one frame every N refreshes, paced by VSync: at
  60 Hz it runs 0.46% faster and every frame lasts exactly one refresh. Sound
  is played that same 0.46% faster (`SDL_SetAudioStreamFrequencyRatio`) with
  a dynamic ±0.5% control that keeps the audio queue at a fixed level
  (40-70 ms), so it neither drains nor fills up (both cases sound like a
  click). If presenting does not wait for VSync (hidden window, a driver that
  ignores it) a clock at the display rate takes over, and if VSync runs at a
  different rate than the system reports (a phone that advertises 120 Hz and
  gives the app 60) the real rate is measured and used. On displays that are
  not a multiple (50, 75, 90, 144 Hz) the GBA clock is followed.
- `frame_blend = true` (*Frame blending*): on displays that cannot be synced,
  each refresh blends the last two frames according to the moment it is shown
  (`PortPacing_BlendAlpha`), with two textures on the GPU. It spreads motion
  evenly at the cost of a slight trail and half a frame of extra latency. It
  does nothing at 60 Hz or with the previous option enabled.

Both enable VSync. Neither touches the logic: the game, its frames and its
sound samples are the same (checked with `mzm_headless`, frames and WAV
byte-for-byte identical); only when frames are presented and the speed at
which sound plays change. `mzm_pacing_test` (built with the headless host and
run in CI) simulates 10 minutes of sound at 60, 59.94, 120 and 59.73 Hz with
sound card clocks off by ±0.1% and buffers of 256 and 1024 samples: the queue
never drains or hits the limit; without the dynamic control, at 60 Hz sound
would be dropped 161 times. For debugging: `MZM_SYNC_LOG=1` prints the game
fps, audio queue level and speed ratio every 5 s, and `MZM_REFRESH_HZ=N` fakes
a different display refresh rate.

**Real position interpolation (not done).** Redrawing intermediate frames with
interpolated background scroll and sprite positions was studied (the software
PPU would allow it by saving each line's registers and the OAM of two frames),
but in this architecture it is not worth it:

- at native resolution (240x160) intermediate positions round to whole
  pixels: at walking speed (1-2 px per frame) the intermediate frame repeats
  one of the two, so it would only help at high speed; half pixels would need
  rendering at a higher resolution (another PPU);
- the game loads the new map column/row into VRAM just off-screen, and with
  the previous frame's scroll already-overwritten tiles would show up at the
  edge;
- the OAM is reordered every frame (sprites change slots), so matching sprites
  between frames is heuristic, and per-line effects (the water's HBlank DMA,
  windows) do not interpolate well.

Frame blending gives half the benefit without any of those artifacts. If the
PPU ever renders at 2x or more, background scroll interpolation would be the
next step.

## Widescreen

The `widescreen` option (`off | auto | 16:10 | 16:9 | 2:1`, Video menu;
`--width N` in headless). The image goes from 240 to 256, 284 or 320 px wide
with the same 160 px height. **The game does not change**: its camera, its
240 px screen, the sprites' "on screen" state (which the AI reads) and the
logic are those of the GBA, so a playthrough or a demo does exactly the same
with or without the option. It just draws more:

- **PPU** (`port/ppu`, `ws_origin`/`ws_obj`): the GBA screen is drawn at
  column `origin` of the wide frame and the backgrounds continue on the sides
  (GBA column `x - origin` + scroll, also with the per-line haze). Windows are
  shifted the same way, and a window edge at the edge of the GBA screen is
  stretched to the edge of the frame. Each OAM entry has its placement: room
  object, HUD anchored left or right, centered label, or "as on the GBA"
  (clipped to its 240 px; whatever is not part of the gameplay in progress).
- **Tilemaps** (`port/game/widescreen.c`): rooms are a ring of 32x16 blocks
  in VRAM that the game fills for its screen (blocks x-2..x+16). Every frame
  the columns missing for the chosen width are rewritten from the room data
  (as `RoomUpdateHorizontalTilemap` does).
- **Sprites**: `SpriteDrawAll_*` also draw the ones in the extra columns
  (`Port_WideSpriteStatus`, without touching their state), leaving 32 OAM
  entries free for Samus and her shots, which are drawn afterwards. Particles
  and projectiles already have enough headroom.
- **Framing** (`port/src/widescreen.c`): the game says where the camera is
  and which part of the room it may show (the current scroll limits, or the
  room without its 2-block border). Normally the GBA screen stays centered;
  near an edge the image stops at the edge and the GBA screen shifts (up to
  `2 * extra` px, smoothed 2 px per frame if the limits change); if the area
  is narrower than the image (vertical shafts, boss rooms) it is centered with
  black on the sides. Nothing the GBA camera could not show is ever shown.
  The HUD is anchored to the edges of the visible area.
- Whatever is not the gameplay loop (title, menus, map, cutscenes, credits)
  is drawn at 240 px in the center. Save states do not change (the thumbnail
  is the center of the image).

With the option off, frames are byte-for-byte identical to before (`MZM_WARP`
run through the 7 areas, the cutscenes, the escape and boot; the only
difference is the Kraid cutscene, whose smoke already varies from run to run
in the previous build). When enabled (16:9), the 240 px GBA part matches the
original image pixel for pixel except for the HUD and labels, which are
repositioned.

## Saving

`struct Sram` has the same size and layout as on the GBA (32,608 bytes). The
only pointer inside it (`EnvironmentalEffect.pOamFrame`, a ROM pointer) is
stored as an opaque `u32`, and active effects are discarded on load because it
cannot be rebuilt on 64-bit. Result: `.sav` files are interchangeable with
mGBA.

## Regions (US, EU, JP)

Each region is a separate build (`-DMZM_REGION=us|eu|jp`, a product flavor on
Android): the game code changes with `#ifdef REGION_*`, data offsets come from
the region's column in `database.json`, and the expected SHA-1 from
`mzm_<region>.sha1`. The header is checked only by the `ZEROMISSION` prefix
(`...E`, `...P`, `...J`). The EWRAM layout in `linker.ld` is the same in all
three.

The JP build compiles, links and passes the smoke test
(`MZM_DEV_SKIP_ROM_CHECK`), and the JP asset table has no overlaps and fits in
the ROM, but **it has not been tested with the Japanese ROM**. Things that are
looked up in the ROM by content and have only been verified with the European
one:

- `sSoundDataEntries`: the first run of 708 valid entries is searched for. The
  table is the same in all regions, sits just before the samples
  (`direct_sound_samples/080908e4.bin`) and is preceded by `music_track_data`,
  with pointers to IWRAM that do not count as valid entries. On JP it should
  come out at `0x0808F31C` (EU: `0x0808FF4C`).
- `ROM_DATA_ARRAY` on arrays compiled into the game, which must appear only
  once in the ROM (expected JP offsets, computed from the neighboring assets in
  `database.json`): `sTourianEscape_47ce20` (`0x47CE7C`),
  `sTourianEscape_47cf50` (`0x47CFAC`), `sSpriteYHalfRadius` (`0x45FD8C`),
  `sBossIcons` (`0x40D254` or `0x40D25C`) and `sRandomNumberTable`
  (`~0x8C9F8`). Extracted arrays (`ROM_DATA`, HUD energy digits...) come from
  the asset table and do not depend on this.
- The Japanese text and fonts (including hiragana) are extracted data with a JP
  offset in `database.json`; the only `SHIFT_JIS()` strings in the code belong
  to the debug menu.

When testing with the JP ROM: the log should say `sound: 708 sounds (table at
0x0808F31C)` and no `Port_RomFind: ... not found` or `... more than once`
should appear; after that, the `MZM_WARP` run under AddressSanitizer (rooms,
cutscenes, escapes) and the map screen in hiragana and katakana.

## Changes to the decompilation

All of them are inside `#ifdef PLATFORM_PC` except two that do not alter the
ROM:

- `static` removed from about 85 data definitions that their header declares
  `extern` (modern GCC rejects it, agbcc does not).
- `.pOamFrame = NULL` → `= 0` in `src/data/demo_data.c` (identical on GBA).

GBA code that makes no sense natively and is replaced:

| Original | Native |
| --- | --- |
| `asm/crt0.s`, `asm/intr_main.s` | `port/src/main.c`, `port/src/frame.c` |
| `asm/syscalls.s` (BIOS) | `port/src/bios.c` |
| `src/dma.c` | `port/src/dma.c` |
| code copies to RAM (SRAM, sound, collisions, haze, Chozodia escape HBlank) | direct calls |
| `asm/audio_internal.s`, `asm/soundcode.s` | `port/game/audio_engine.c` (C translation) |
| `MultiBootWaitCycles` (link cable) | no-op |
| embedded NES Metroid | not included |
