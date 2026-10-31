/* p4_osd.h: the side panel's menu area, above the on-screen keyboard
   (PLAN.md Phase 3, "OSD").

   The top 240 lines of the side panel, or all of it while the keyboard is
   hidden, as P4_OSD_ROWS rows of text in a proportional typeface,
   anti-aliased (Aileron, p4_osd_font.h: 16 pixels in the short rows, 24
   in the tall). A tab moves on to the next fifth of the width, for
   things that line up; text past the right edge is cut off. What
   the rows say, and what a tap does, is up to the caller (osd.py): this
   only draws text and reports taps. Text may be set from another core
   while the display draws; a row changed mid-draw is drawn again the next
   frame. */

#ifndef P4_OSD_H
#define P4_OSD_H

#include <stdint.h>

#include "p4_touch.h"

#define P4_OSD_ROWS  10
#define P4_OSD_CHARS 40                 /* the most a row keeps */
#define P4_OSD_TAB   46                 /* tab stops, in pixels */

enum {
  P4_OSD_PLAIN,                 /* white on black */
  P4_OSD_INVERSE,               /* black on white: a title, a selection */
  P4_OSD_BUTTON,                /* white on grey, like a key */
};

/* Added to a style: the small size even in tall rows, for a line that
   would run past the edge (the status line) */
#define P4_OSD_SMALL 0x10

/* Rows already set are kept, and drawn again into every frame buffer */
void p4_osd_init( void );

/* Row 0 to P4_OSD_ROWS - 1: up to P4_OSD_CHARS characters (more are cut
   off) in a style */
void p4_osd_text( int row, const char *text, int style );

/* Tall rows (1) fill the side panel's whole height, 60 lines each, for
   when the keyboard is hidden (p4_keyboard_show); short ones (0, the
   default) fill the 240 lines above it. Every row is drawn again. */
void p4_osd_tall( int tall );

/* Note a finger landing in the menu area (one that wasn't down there the
   frame before) as a tap. Called each frame with all the touches. */
void p4_osd_touch( const p4_touch_point_t *points, int count );

/* The last tap not yet taken: 1 and its row and x, in pixels from the
   menu's left edge (tab stops are multiples of P4_OSD_TAB), or 0 */
int p4_osd_tap( int *row, int *x );

/* For the display's side-panel hook, with p4_keyboard_draw */
int p4_osd_draw( uint16_t *fb, int buffer, int shown );

#endif /* P4_OSD_H */
