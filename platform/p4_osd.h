/* p4_osd.h: the side panel's menu area, above the on-screen keyboard
   (PLAN.md Phase 3, "OSD").

   The top 240 lines of the side panel, or all of it while the keyboard is
   hidden, as P4_OSD_ROWS rows of P4_OSD_COLS characters, drawn in the
   ROM's character set at twice its size. What
   the rows say, and what a tap does, is up to the caller (osd.py): this
   only draws text and reports taps. Text may be set from another core
   while the display draws; a row changed mid-draw is drawn again the next
   frame. */

#ifndef P4_OSD_H
#define P4_OSD_H

#include <stdint.h>

#include "p4_touch.h"

#define P4_OSD_ROWS 10
#define P4_OSD_COLS 31

enum {
  P4_OSD_PLAIN,                 /* white on black */
  P4_OSD_INVERSE,               /* black on white: a title, a selection */
  P4_OSD_BUTTON,                /* white on grey, like a key */
};

/* font: as p4_keyboard_init's. Copied. Rows already set are kept, and
   drawn again into every frame buffer. */
void p4_osd_init( const uint8_t *font );

/* Row 0 to P4_OSD_ROWS - 1: up to P4_OSD_COLS characters (more are cut
   off, fewer padded with spaces) in a style */
void p4_osd_text( int row, const char *text, int style );

/* Tall rows (1) fill the side panel's whole height, 60 lines each, for
   when the keyboard is hidden (p4_keyboard_show); short ones (0, the
   default) fill the 240 lines above it. Every row is drawn again. */
void p4_osd_tall( int tall );

/* Note a finger landing in the menu area (one that wasn't down there the
   frame before) as a tap. Called each frame with all the touches. */
void p4_osd_touch( const p4_touch_point_t *points, int count );

/* The last tap not yet taken: 1 and its row and column, or 0 */
int p4_osd_tap( int *row, int *col );

/* For the display's side-panel hook, with p4_keyboard_draw */
int p4_osd_draw( uint16_t *fb, int buffer, int shown );

#endif /* P4_OSD_H */
