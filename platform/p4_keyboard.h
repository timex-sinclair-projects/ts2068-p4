/* p4_keyboard.h: the on-screen TS2068 keyboard, in the side panel (PLAN.md
   Phase 3, "Input, stage 1").

   The 40 keys in four rows of ten, as on the TS2068, in the bottom 360
   lines of the side panel; the 240 above are left for the OSD. Each
   touch presses the key under it, so CAPS SHIFT or SYMBOL SHIFT and a
   key are two fingers. A key shows inverted while it's down. The labels
   are drawn in the ROM's own character set. */

#ifndef P4_KEYBOARD_H
#define P4_KEYBOARD_H

#include <stdint.h>

#include "p4_touch.h"

/* font: the HOME ROM's 96 characters from space, 8 bytes each (0x3D00 in
   the TS2068 ROM). Copied; all keys up. */
void p4_keyboard_init( const uint8_t *font );

/* Press the keys under the touches and release the rest; rows gets the
   eight half-rows for ts2068_set_keyboard. */
void p4_keyboard_touch( const p4_touch_point_t *points, int count,
                        uint8_t rows[8] );

/* Show the keyboard (1, the default) or hide it (0), for a physical
   keyboard. Hidden, it draws nothing and no touch presses a key; the menu
   takes the whole side panel (p4_osd_tall). Shown again, it's drawn
   whole into each buffer. */
void p4_keyboard_show( int show );

/* The keys down, bit row * 10 + column, top left first */
uint64_t p4_keyboard_down( void );

/* The display's side-panel hook (p4_display_set_side) */
int p4_keyboard_draw( uint16_t *fb, int buffer, int shown );

#endif /* P4_KEYBOARD_H */
