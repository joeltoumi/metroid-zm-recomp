# Notas técnicas del port

## Cómo arranca

1. `port/src/main.c` (`Port_Main`) sustituye a `asm/crt0.s`: inicia el host,
   carga la ROM, resetea la "consola" y llama a `agbmain()` en bucle (el juego
   sale de `agbmain` al hacer *soft reset*).
2. El bucle del juego termina cada frame con `SYSCALL(2)` (Halt). En nativo eso
   es `Port_RunFrame()` (`port/src/frame.c`), que reproduce un frame de
   hardware: VBlank (DMA de VBlank + IRQ de VBlank del juego), las 160 líneas
   visibles con sus eventos de HBlank/VCount, el render del PPU, presentar,
   leer controles y esperar al siguiente tick (59,73 Hz).

## Memoria

`include/gba/memory.h` redefine `EWRAM_BASE`, `REG_BASE`, `VRAM_BASE`... con
`PLATFORM_PC` para que apunten a buffers del host (`gPortEwram`, `gPortIo`...).
El código del juego no cambia: escribe en registros como siempre y el PPU lee
esos mismos buffers. Los símbolos que `linker.ld` coloca en EWRAM en direcciones
fijas se convierten en macros sobre `gPortEwram` + offset, así siguen
solapándose como en la GBA.

Lo que no cabe en un registro de 32 bits (punteros de 64 bits en DMA) pasa por
`Port_DmaSet` en lugar de escribir los registros DMA.

Las esperas activas sobre `REG_VCOUNT` avanzan un contador virtual
(`READ_16` en `include/io.h`).

## Datos del juego

`tools/port/gen_assets.py` (lo ejecuta CMake):

- escribe stubs `.inc` llenos de ceros con el número exacto de elementos de
  `database.json`, así cada array tiene su tamaño real;
- preprocesa cada `src/data/**.c` para ver qué `#include "extracted/..."` están
  activos en la región elegida y genera un wrapper que incluye el original y
  registra (dirección del array, offset en la ROM, tamaño) en un constructor;
- los wrappers se compilan con `-Dconst=` para que los arrays sean escribibles
  (casi todos acaban en `.bss`).

Al arrancar, `port/src/assets.c` verifica el SHA-1 de la ROM y copia cada
bloque. `MZM_DEV_SKIP_ROM_CHECK=1` permite arrancar con una imagen falsa
(solo para el smoke test de CI).

## Guardado

`struct Sram` tiene el mismo tamaño y layout que en la GBA (32 608 bytes). El
único puntero que había dentro (`EnvironmentalEffect.pOamFrame`, un puntero a
ROM) se guarda como un `u32` opaco y los efectos activos se descartan al cargar,
porque en 64 bits no puede reconstruirse. Resultado: los `.sav` son
intercambiables con mGBA.

## Cambios en la decompilación

Todos van entre `#ifdef PLATFORM_PC` salvo dos que no alteran la ROM:

- `static` eliminado de unas 85 definiciones de datos que su cabecera declara
  `extern` (GCC moderno lo rechaza, agbcc no).
- `.pOamFrame = NULL` → `= 0` en `src/data/demo_data.c` (idéntico en GBA).

Código de la GBA que no tiene sentido en nativo y se sustituye:

| Original | Nativo |
| --- | --- |
| `asm/crt0.s`, `asm/intr_main.s` | `port/src/main.c`, `port/src/frame.c` |
| `asm/syscalls.s` (BIOS) | `port/src/bios.c` |
| `src/dma.c` | `port/src/dma.c` |
| copias de código a RAM (SRAM, sonido) | llamadas directas |
| `asm/audio_internal.s`, `asm/soundcode.s` | `port/src/audio_stubs.c` (silencio, ver M3) |
| `MultiBootWaitCycles` (cable link) | no-op |
| Metroid NES embebido | no incluido |
