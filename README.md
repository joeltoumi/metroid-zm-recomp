# mzm-android

Port nativo de **Metroid: Zero Mission** (GBA) a Android y PC, construido sobre
la decompilación [metroidret/mzm](https://github.com/metroidret/mzm).

No es un emulador: el código C del juego se compila directamente para ARM64 /
x86-64 y la "consola" (vídeo, DMA, interrupciones, BIOS) la pone una capa fina
en `port/`.

> **Estado: en desarrollo temprano.** El juego compila, arranca y suena en
> nativo; falta validar todo el recorrido con la ROM real.
> Ver [docs/ROADMAP.md](docs/ROADMAP.md).

## Necesitas tu propia ROM

El repositorio y los APK **no contienen ningún dato del juego**. Al arrancar por
primera vez la app te pide tu dump de *Metroid - Zero Mission*. Hay un build
por región, y cada uno solo acepta su ROM:

| Región | Build | SHA-1 |
| --- | --- | --- |
| Europa | `eu` | `0fd107445a42e6f3a3e5ce8c865f412583179903` |
| USA | `us` | `5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8` |

La ROM se copia al almacenamiento privado de la app y los gráficos, niveles y
textos se cargan de ella en cada arranque. La versión europea arranca con la
selección de idioma, como en la GBA.

## Instalar en Android

Cada push a `main` genera dos APK en **Actions → build → Artifacts →
`mzm-android-apk`** (arm64 + x86_64): `app-eu-release.apk` para la ROM europea
y `app-us-release.apk` para la americana. Se pueden instalar los dos a la vez.
Instala el de tu región, abre *Zero Mission (EU)* o *(US)* y elige tu `.gba`
con el selector de archivos.

Para compilar solo uno: `./gradlew assembleEuRelease` o `assembleUsRelease`.

## Compilar en PC (Linux)

Requisitos: CMake ≥ 3.22, Ninja, Python 3, un compilador C11 y SDL3 (si no está
instalado, CMake lo descarga y compila).

```sh
cmake -B build -G Ninja                 # ROM USA
cmake -B build -G Ninja -DMZM_REGION=eu # ROM europea
cmake --build build
./build/mzm ruta/a/tu_rom.gba
```

En PC todos los builds comparten carpeta de datos, así que cada región guarda
su partida y su copia de la ROM por separado (`mzm_eu.sav`, `rom_eu.gba`).

Sin ventana (pruebas/CI): `-DMZM_HOST=headless`, que vuelca fotogramas a PPM:

```sh
./build/mzm_headless --rom mzm_us_baserom.gba --frames 1200 --dump-every 60 \
    --press 400:S:5      # pulsa Start en el frame 400 durante 5 frames
```

`--state-save FRAME:FICHERO` y `--state-load FRAME:FICHERO` guardan o cargan un
estado en ese frame (para pruebas reproducibles). `--width 284` dibuja en
panorámico (16:9).

## Controles y ajustes

El menú de opciones pausa el juego y se abre con **Esc** o **F1**, con
**Select+Start** (o el botón Guía) en el mando, con el botón **Atrás** de
Android o con el botón `=` de arriba del mando táctil. Desde ahí:

- **Guardados rápidos**: 4 ranuras con miniatura y fecha. `F5` guarda y `F8`
  carga la última ranura usada. No tocan la partida normal (`.sav`).
- **Controles**: cambiar cualquier botón del teclado o del mando (pulsa la
  casilla y luego la tecla/botón nuevo) y la zona muerta del stick.
- **Vídeo**: pantalla completa, escalado entero, filtro suave, efecto LCD
  (rejilla de píxeles) o CRT (líneas de barrido), VSync y **pantalla
  panorámica** (16:10, 16:9, 2:1 o como la pantalla; desactivada por
  defecto): se ve más sala a los lados en vez de bandas negras, sin cambiar
  el juego. Menús, mapa y cinemáticas siguen en 3:2 en el centro.
- **Sonido y velocidad**: volumen, velocidad del avance rápido (x2-x10) y si
  su botón se mantiene o activa/desactiva.
- **Mando táctil**: cuándo se muestra, opacidad y un editor para mover cada
  botón, cambiar su tamaño y qué botón de la GBA pulsa.

Los ajustes se guardan en `settings.ini`, en la carpeta de datos de la app
(Linux: `~/.local/share/mzm-android/mzm/`), que también se puede editar a mano.
Se crea con los valores por defecto la primera vez.

| GBA | Teclado | Mando |
| --- | --- | --- |
| A / B | X / Z | botón derecho / inferior |
| L / R | A / S | bumpers o gatillos |
| Start / Select | Enter / Retroceso | Start / Back |
| Cruceta | flechas | cruceta o stick izquierdo |
| Avance rápido | Tab (mantener) | clic del stick derecho |

`F11` alterna la pantalla completa. En pantallas táctiles aparece un mando
virtual (`touch_controls = auto | on | off`) que se oculta al usar un mando
físico, como los controles de la AYN Thor. Select+Start abre el menú; el reset
del juego (A+B+Select+Start) sigue funcionando si A y B se pulsan antes.

Las partidas se guardan en `mzm.sav` con el mismo formato que la GBA, así que
un `.sav` de mGBA de la versión USA sirve tal cual.

## Cómo está hecho

- `src/`, `include/`: la decompilación. Los cambios para el build nativo van
  entre `#ifdef PLATFORM_PC` y no alteran la ROM de GBA.
- `port/src/`: memoria emulada, DMA, BIOS en C, bucle de frame con IRQs de
  VBlank/HBlank/VCount, carga de la ROM, save states y frontends (SDL3 con el
  menú en [Dear ImGui](https://github.com/ocornut/imgui), y headless).
- `port/ppu/`: PPU por software de [Project Picori](https://github.com/999sian/tmc)
  (el port nativo de Minish Cap).
- `tools/port/gen_assets.py`: compila los datos de `src/data` a cero y genera
  el registro que los rellena desde la ROM al arrancar.
- `android/`: proyecto Gradle (SDL3 + `libmain.so`).

Detalles en [docs/PORT.md](docs/PORT.md).

## Licencias

La decompilación es MIT ([LICENSE](LICENSE)); la capa del port es GPL-3.0
([port/LICENSE](port/LICENSE)). Metroid es propiedad de Nintendo; este proyecto
no incluye ni distribuye nada de su propiedad.

El README original de la decompilación está en
[docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).
