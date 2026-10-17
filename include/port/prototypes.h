#ifndef PORT_PROTOTYPES_H
#define PORT_PROTOTYPES_H

/*
 * mzm-android: force-included in every game translation unit of the native
 * build (see CMakeLists.txt).
 *
 * Many decomp files call functions without including their header: agbcc
 * accepts implicit declarations, and on the GBA (32-bit int/pointers) they
 * happen to work. On a 64-bit host they don't: an implicitly declared
 * function "returns int", which truncates pointers and leaves the upper bits
 * of u8/u16 results undefined. So every function prototype is made visible.
 *
 * Generated list of headers (tools/port: see docs/PORT.md), plus the few
 * functions that have no prototype anywhere.
 */

#include "types.h"
#include "animated_graphics.h"
#include "audio/track_internal.h"
#include "audio_wrappers.h"
#include "bg_clip.h"
#include "block.h"
#include "callbacks.h"
#include "chozodia_escape.h"
#include "clipdata.h"
#include "color_effects.h"
#include "color_fading.h"
#include "complex_oam.h"
#include "connection.h"
#include "cutscenes/cutscene_utils.h"
#include "demo.h"
#include "dma.h"
#include "ending_and_gallery.h"
#include "escape.h"
#include "fusion_gallery.h"
#include "haze.h"
#include "hud.h"
#include "in_game.h"
#include "in_game_cutscene.h"
#include "init_game.h"
#include "init_helpers.h"
#include "intro.h"
#include "location_text.h"
#include "menus/file_select.h"
#include "menus/game_over.h"
#include "menus/pause_screen.h"
#include "menus/pause_screen_sub_menus.h"
#include "menus/status_screen.h"
#include "menus/title_screen.h"
#include "minimap.h"
#include "music_wrappers.h"
#include "oam_id.h"
#include "particle.h"
#include "projectile_util.h"
#include "room.h"
#include "room_music.h"
#include "samus.h"
#include "save_file.h"
#include "save_file_load.h"
#include "screen_shake.h"
#include "scroll.h"
#include "soft_reset.h"
#include "soft_reset_input.h"
#include "sprite.h"
#include "sprite_debris.h"
#include "sprite_util.h"
#include "sprites_ai/chozo_statue.h"
#include "sprites_ai/chozo_statue_movement.h"
#include "sprites_ai/save_platform.h"
#include "sram/sram.h"
#include "syscall_wrappers.h"
#include "syscalls.h"
#include "text.h"
#include "time_attack.h"
#include "transparency.h"
#include "update_input.h"

#include "sprites_ai/black_space_pirate.h"
#include "menus/erase_sram.h"
#include "cable_link.h"
#include "link.h"
#include "display.h"
#include "power_bomb_explosion.h"
/* Defined in C but declared in no header. */
u32 GadoraHatchUpdate(u16 xPosition, u16 yPosition, u8 action);
u32 RidleyCheckGrabbing(u16 yPosition, u16 xPosition);
boolu32 ProcessPauseButtonPress(void);
void InGameTimerUpdate(void);
/* asm/soundcode.s */
s32 CallGetNoteFrequency(s32 frequency, u32 pitch);

#endif /* PORT_PROTOTYPES_H */
