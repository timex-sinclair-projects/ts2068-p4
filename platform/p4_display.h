/* p4_display.h: the CrowPanel Advanced 7"'s 1024x600 MIPI-DSI panel
   (EK79007), for the TS2068's picture.

   The TS2068's 640x240 picture is drawn with every line doubled, 640x480,
   on the left of the panel, with 60 lines above and below; the 384 pixels
   to the right are the side panel (PLAN.md Phase 3, "Display"). Three
   frame buffers in PSRAM, flipped at the panel's refresh, so a frame never
   tears and drawing never waits. The panel refreshes at 60.10 Hz. */

#ifndef P4_DISPLAY_H
#define P4_DISPLAY_H

#include <stdint.h>

#include "esp_err.h"

#define P4_DISPLAY_WIDTH  1024
#define P4_DISPLAY_HEIGHT 600

/* Where the doubled TS2068 picture goes */
#define P4_DISPLAY_TV_X 0
#define P4_DISPLAY_TV_Y 60

/* Power the DSI PHY, start the panel, clear both frame buffers and turn
   the backlight on. The side panel shows the 16 TS2068 colours as bars,
   top to bottom 0-7 then 8-15 (black, blue, red, magenta, green, cyan,
   yellow, white), until something else draws there: a check that the
   colours and byte order are right. Call once; later calls do nothing. */
esp_err_t p4_display_init( void );

/* Backlight, 0 (off) to 100 */
void p4_display_backlight( int percent );

/* Draws one line, 0-239, of the TS2068's 640x240 picture as colour
   indices 0-15: ts2068_render_line() */
typedef void (*p4_display_line_fn)( int line, uint8_t *pixels );

/* Show a TS2068 frame, taken from render_line a line at a time. Drawn
   into a frame buffer that's neither on screen nor waiting for the next
   refresh, and shown from that refresh; only the lines that buffer
   doesn't already hold are converted and copied. Returns 0 if drawn, or 1
   if the frame is the same as the last one shown. Waits only for the PPA
   to finish its copies, never for the panel. */
int p4_display_ts2068( p4_display_line_fn render_line );

typedef struct {
  uint32_t refreshes;           /* panel refreshes since init */
  uint32_t drawn, unchanged;    /* p4_display_ts2068 results */
  uint32_t replaced;            /* drawn over a frame never shown */
  uint64_t lines;               /* TS2068 lines converted, in all */
  uint32_t strips;              /* strips the PPA copied */
  uint32_t draw_us_max;         /* longest conversion and flip */
  uint32_t flip_us_max;         /* longest flip (the cache write-back) */
  uint64_t draw_us_total;
} p4_display_stats_t;

void p4_display_get_stats( p4_display_stats_t *stats );

/* Compare the picture in the frame buffer last flipped, all 480 rows,
   with render_line's: the number of pixels that differ, or -1 before
   init. Only meaningful while the machine is stopped. */
int p4_display_check( p4_display_line_fn render_line );

/* Nonzero: convert every line of every frame, as if all had changed. For
   measuring the worst case. */
void p4_display_redraw_all( int on );

#endif /* P4_DISPLAY_H */
