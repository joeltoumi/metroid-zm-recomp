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

### Lecturas más allá del final de un array

Hay sitios donde el juego lee más bytes de los que tiene el array (copias de
tamaño fijo, índices que se pasan: el mapa, las tablas de colisión, la paleta
común, la intro, la huida de Tourian...). En la GBA eso lee los datos que
siguen en la ROM; en nativo cada array es una variable aparte. Para leer lo
mismo:

- `ROM_DATA(ptr)` (`include/macros.h`) devuelve el mismo dato dentro de la
  copia de la ROM, usando la tabla de assets (`Port_RomMirror`).
- `ROM_DATA_ARRAY(array)` hace lo mismo con arrays que están compilados en el
  juego en vez de extraídos: los localiza en la ROM por su contenido
  (`Port_RomFind`, que exige que aparezcan una sola vez).

Cuando lo que se lee de más no se usa nunca (las tablas de colisión, las
paletas de Samus fuera de su pose, un escombro sin hueco libre) se recorta la
lectura o se descarta la escritura.

## Sonido

El motor de MZM es casi todo C (`src/audio.c`, `src/audio_wrappers.c`,
`src/music_wrappers.c`); sus rutinas internas y los bucles de mezcla estaban
en ensamblador ARM y están traducidos en `port/game/audio_engine.c`,
manteniendo la aritmética exacta.

Las pistas, los voicegroups y las muestras son datos en ensamblador con
punteros de 32 bits entre ellos, así que no se copian: la ROM del jugador se
queda en memoria y el motor lee los scripts y las muestras en su sitio
(`Port_RomPointer`). Solo se reconstruye lo que cambia de forma en 64 bits:

- `sSoundDataEntries` (punteros a las cabeceras de pista). Su dirección
  cambia según la región, así que se localiza en la ROM: es la única
  secuencia de 708 entradas válidas seguidas.
- Los voicegroups (`struct Voice` ocupa 12 bytes en la GBA y 24 en nativo).
- Los punteros dentro de los scripts (lista de pistas, GOTO/PATT) se traducen
  al leerlos.

El hardware lo emula `port/src/apu.c`: cada frame reproduce las muestras que
la DMA 1/2 llevaría a los FIFO A/B (lanzando la IRQ de DMA 2 cada 16, como la
consola) y los cuatro canales PSG a partir de sus registros. Sale a 48 kHz
estéreo por `SDL_AudioStream`; el host headless puede grabarlo con `--wav`.

Se ha comparado con mGBA grabando lo mismo en los dos (intro y Brinstar con
la ROM europea): la forma de onda coincide (correlación 0,98-0,99 en la
música de Brinstar) y el volumen también.

## Save states

`port/src/savestate.c`. El estado de la máquina es la memoria de la GBA
(EWRAM, IWRAM, I/O, paletas, VRAM, OAM; la SRAM no, para no tocar la partida
real) más la sección `mzm_state`: con `PLATFORM_PC`, `IWRAM_DATA` mete ahí
todas las variables globales del juego, y `PORT_STATE` las del port que forman
parte del "hardware" (canales DMA, chip de sonido).

Solo se guarda o carga en el `Halt` del final del bucle de `agbmain` (el único
`SWI 2` del juego; el host recibe `Port_HostFrameBoundary`), donde la pila de
C es siempre la misma y no hace falta guardarla. Por eso el menú se ejecuta
ahí, con el juego en pausa.

En nativo muchas variables son punteros (al código y datos del ejecutable o a
la ROM del jugador, que el motor de sonido lee en su sitio), y esas direcciones
cambian en cada ejecución (ASLR, `malloc`). El estado guarda dónde estaban y al
cargar desliza cada valor de 64 bits que cae en uno de esos rangos. Un estado
de otro build (otra versión de la app) se rechaza. Los voicegroups convertidos
se reconstruyen en el mismo orden a partir de sus direcciones.

Probado con `mzm_headless`: un estado guardado en un proceso y cargado en otro
produce exactamente los mismos fotogramas, y los dos estados difieren solo en
los punteros reubicados.

## Modo de prueba (warp)

Para probar zonas sin jugar hasta ellas, la variable de entorno `MZM_WARP`
empieza una partida nueva directamente en una sala, con todos los objetos
(`port/src/warp.c` y `port/game/warp.c`):

```sh
MZM_WARP=kraid:3 ./mzm                      # una sala
MZM_WARP=norfair:* MZM_WARP_FRAMES=150 \
  ./mzm_headless --dump-every 150 ...        # todas las salas de una zona
MZM_WARP=cutscene:* MZM_WARP_FRAMES=1500 ...  # las 14 cinemáticas
MZM_WARP=tourianescape:0                     # también chozodiaescape, credits
```

Las zonas van por nombre (`brinstar`, `kraid`, `norfair`, `ridley`,
`tourian`, `crateria`, `chozodia`) o número. Con varias salas, cada una dura
`MZM_WARP_FRAMES` frames (300 por defecto).

Con esto se han cargado las 324 salas de las 7 zonas, las 14 cinemáticas, las
dos huidas, los créditos y las demos de la pantalla de título con
AddressSanitizer, sin errores.

## Pantalla panorámica

Opción `widescreen` (`off | auto | 16:10 | 16:9 | 2:1`, menú Vídeo; `--width N`
en headless). La imagen pasa de 240 a 256, 284 o 320 px de ancho con los
mismos 160 de alto. **El juego no cambia**: su cámara, su pantalla de 240 px,
el estado "en pantalla" de los sprites (que lee la IA) y la lógica son los de
la GBA, así que una partida o una demo hace exactamente lo mismo con la opción
o sin ella. Solo se dibuja más:

- **PPU** (`port/ppu`, `ws_origin`/`ws_obj`): la pantalla de la GBA se dibuja
  en la columna `origin` del fotograma ancho y los fondos siguen a los lados
  (columna GBA `x - origin` + scroll, también con el haze por línea). Las
  ventanas se desplazan igual, y un borde de ventana en el borde de la
  pantalla GBA se estira hasta el borde del fotograma. Cada entrada de OAM
  tiene su colocación: objeto de la sala, HUD pegado a la izquierda o a la
  derecha, rótulo centrado, o "como en la GBA" (recortada a sus 240 px; lo
  que no es del juego en curso).
- **Tilemaps** (`port/game/widescreen.c`): las salas son un anillo de 32x16
  bloques en VRAM que el juego rellena para su pantalla (bloques x-2..x+16).
  Cada frame se reescriben desde los datos de la sala las columnas que faltan
  para el ancho elegido (como hace `RoomUpdateHorizontalTilemap`).
- **Sprites**: `SpriteDrawAll_*` dibujan también los que están en las
  columnas extra (`Port_WideSpriteStatus`, sin tocar su estado), dejando 32
  entradas de OAM libres para Samus y los disparos, que se dibujan después.
  Partículas y proyectiles ya tienen margen suficiente.
- **Encuadre** (`port/src/widescreen.c`): el juego dice dónde está la cámara
  y qué parte de la sala puede enseñar (los límites de scroll actuales, o la
  sala sin su borde de 2 bloques). Normalmente la pantalla GBA queda centrada;
  cerca de un borde la imagen se para en el borde y la pantalla GBA se
  desplaza (hasta `2 * extra` px, suavizado 2 px por frame si cambian los
  límites); si la zona es más estrecha que la imagen (pozos verticales,
  salas de jefe) se centra con negro a los lados. Nunca se enseña lo que la
  cámara de la GBA no podría enseñar. El HUD va pegado a los bordes de lo
  visible.
- Lo que no es el bucle de juego (título, menús, mapa, cinemáticas,
  créditos) se dibuja a 240 px en el centro. Los save states no cambian
  (la miniatura es el centro de la imagen).

Con la opción desactivada los fotogramas son idénticos byte a byte a los de
antes (recorrido de `MZM_WARP` por las 7 zonas, las cinemáticas, la huida y el
arranque; la única diferencia es la cinemática de Kraid, cuyo humo ya varía de
una ejecución a otra en el build anterior). Activada (16:9), la parte de 240 px
de la GBA coincide píxel a píxel con la imagen original salvo el HUD y los
rótulos, que se recolocan.

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
| copias de código a RAM (SRAM, sonido, colisiones, haze, HBlank de la huida de Chozodia) | llamadas directas |
| `asm/audio_internal.s`, `asm/soundcode.s` | `port/game/audio_engine.c` (traducción a C) |
| `MultiBootWaitCycles` (cable link) | no-op |
| Metroid NES embebido | no incluido |
