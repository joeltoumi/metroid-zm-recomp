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
- [x] Validar con la ROM real (EU, en headless): idioma, intro, título,
      selección de partida, primera sala de Brinstar y mapa de pausa
- [x] Modo de prueba `MZM_WARP` para cargar cualquier sala, cinemática o
      huida (ver docs/PORT.md)
- [x] Las 324 salas, las 14 cinemáticas, las huidas, los créditos y las demos
      se cargan y se ven bien (ROM EU)
- [x] Lecturas y escrituras fuera de rango que detecta AddressSanitizer
      (salas, mapa, intro, pausa, paletas de Samus, escombros, huida de
      Tourian): ahora leen de la ROM como en la GBA
- [ ] Partida completa jugada a mano (en el móvil) para encontrar lo que las
      pruebas automáticas no ven

## M3 — Sonido ✅

- [x] Traducir `asm/audio_internal.s` y `asm/soundcode.s` a C
- [x] Datos de sonido (`sound/*.s`) leídos de la ROM del jugador
- [x] Emular los canales PSG (1-4) y el FIFO A/B; salida por SDL_AudioStream
- [x] Comparado con mGBA (intro y Brinstar, ROM EU)

## M4 — Menú de opciones en el juego ✅

- [x] Overlay (ImGui) accesible con Select+Start, Esc/F1, Atrás o un botón táctil
- [x] Remapeo de controles desde el menú (teclado, mando, táctil)
- [x] Vídeo: escalado entero, filtro, efectos LCD/CRT, sincronización
- [x] Guardados rápidos (save states, 4 ranuras con miniatura) y avance
      rápido configurable (velocidad, mantener/alternar)
- [x] Editor de posición/tamaño/opacidad del mando táctil
- [ ] Probar el menú con mando y táctil en un dispositivo real (AYN Thor)

## M5 — Mejoras

- [x] Pantalla panorámica (como Picori con Minish Cap): opcional, desactivada
      por defecto
- [x] 60 FPS de lógica / interpolación: sincronización con pantallas de
      60/120 Hz (sonido reajustado) y mezcla de fotogramas en 90/144 Hz
      (opcionales; la interpolación real de posiciones se descartó, ver
      docs/PORT.md)
- [x] Segunda pantalla de la AYN Thor: mapa o estado de Samus
- [x] Vibración
- [x] Build EU (APK propio, CI con las dos regiones)
- [x] Build JP (APK propio y CI; sin probar aún con la ROM japonesa real)

## No soportado

- Metroid original (NES) desbloqueable: es un emulador ARM de GBA embebido.
- Conexión con Metroid Fusion por cable link.
