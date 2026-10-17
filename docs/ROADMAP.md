# Hoja de ruta

## M1 — Build nativo ✅

- [x] La decompilación compila en x86-64 / ARM64 (`PLATFORM_PC`)
- [x] Mapa de memoria de la GBA en buffers del host
- [x] DMA (inmediata, HBlank, VBlank), BIOS en C, bucle de frame con IRQs
- [x] PPU por software, host headless, smoke test en CI
- [x] Guardado con el layout exacto de la GBA (32 KiB, compatible con mGBA)

## M2 — Jugable en PC y Android (en curso)

- [x] Datos del juego cargados de la ROM del jugador (verificada por SHA-1)
- [x] Frontend SDL3: escalado, pantalla completa, teclado, mando, táctil,
      avance rápido, `settings.ini`
- [x] Proyecto Android + APK en CI
- [ ] Validar con la ROM real: intro, título, selección de partida, primera
      sala de Brinstar
- [ ] Recorrer el juego y arreglar lo que falle (efectos de raster, modos de
      vídeo, cinemáticas)

## M3 — Sonido

El motor de audio de MZM es mayoritariamente C (`src/audio.c`), pero sus
rutinas internas y los bucles de mezcla siguen en ensamblador ARM
(`asm/audio_internal.s`, `asm/soundcode.s`) y los datos de las pistas son
ensamblador (`sound/*.s`).

- [ ] Traducir `asm/audio_internal.s` y `asm/soundcode.s` a C
- [ ] Convertir `sound/*.s` a datos C (las muestras PCM se cargan de la ROM)
- [ ] Emular los canales PSG (1-4) y el FIFO A/B; salida por SDL_AudioStream

## M4 — Menú de opciones en el juego

- [ ] Overlay (ImGui) accesible con Select+Start o un botón táctil
- [ ] Remapeo de controles desde el menú (teclado, mando, táctil)
- [ ] Vídeo: escalado entero, filtro, shaders LCD/CRT, sincronización
- [ ] Guardados rápidos (save states) y avance rápido configurable
- [ ] Editor de posición/tamaño/opacidad del mando táctil

## M5 — Mejoras

- [ ] Pantalla panorámica (como Picori con Minish Cap)
- [ ] 60 FPS de lógica / interpolación
- [ ] Segunda pantalla de la AYN Thor: mapa o estado de Samus
- [ ] Vibración
- [x] Build EU (APK propio, CI con las dos regiones)
- [ ] Build JP

## No soportado

- Metroid original (NES) desbloqueable: es un emulador ARM de GBA embebido.
- Conexión con Metroid Fusion por cable link.
