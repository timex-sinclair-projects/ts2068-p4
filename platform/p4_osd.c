/* p4_osd.c: the side panel's menu area (p4_osd.h).

   Each row carries a version, bumped when its text changes; each frame
   buffer remembers the version of each row it holds, and the draw brings
   the rows that differ up to date, as p4_keyboard.c does for keys. */

#include <stdbool.h>
#include <string.h>

#include "esp_cache.h"

#include "p4_display.h"
#include "p4_osd.h"
#include "p4_osd_font.h"

#define AREA_X  ( P4_DISPLAY_WIDTH - 2 - 230 )  /* lined up with the keyboard */
#define AREA_W  230
#define ROW_H_SHORT 24                          /* above the keyboard */
#define ROW_H_TALL  ( P4_DISPLAY_HEIGHT / P4_OSD_ROWS )  /* the whole panel */
#define MARGIN  8                               /* text's left edge */
#define TAB     P4_OSD_TAB                      /* AREA_W / 5 */

#define COLOUR_BLACK 0x0000
#define COLOUR_GREY  0x3186                     /* the keys' face */
#define COLOUR_WHITE 0xffff

static bool ready;
static struct {
  char text[ P4_OSD_CHARS ];
  uint8_t style;
  volatile uint32_t version;
} rows[ P4_OSD_ROWS ];
static uint32_t held[ P4_DISPLAY_BUFFERS ][ P4_OSD_ROWS ];
static bool was_down;
static volatile int tap_row = -1, tap_x;
static volatile int row_h = ROW_H_SHORT;

void
p4_osd_init( void )
{
  memset( held, 0, sizeof( held ) );    /* versions start at 1: all redrawn */
  for( int r = 0; r < P4_OSD_ROWS; r++ ) {
    if( !rows[r].version ) memset( rows[r].text, 0, P4_OSD_CHARS );
    rows[r].version++;
  }
  tap_row = -1;
  ready = true;
}

void
p4_osd_text( int row, const char *text, int style )
{
  if( row < 0 || row >= P4_OSD_ROWS ) return;
  int n = strnlen( text, P4_OSD_CHARS );
  memcpy( rows[ row ].text, text, n );
  memset( rows[ row ].text + n, 0, P4_OSD_CHARS - n );
  rows[ row ].style = style;
  __sync_synchronize();                 /* the text before the version */
  rows[ row ].version++;
}

void
p4_osd_tall( int tall )
{
  int h = tall ? ROW_H_TALL : ROW_H_SHORT;
  if( h == row_h ) return;
  row_h = h;
  __sync_synchronize();                 /* the height before the versions */
  for( int r = 0; r < P4_OSD_ROWS; r++ ) rows[r].version++;
}

void
p4_osd_touch( const p4_touch_point_t *points, int count )
{
  bool down = false;
  int h = row_h;
  for( int i = 0; i < count; i++ ) {
    int x = points[i].x - AREA_X, y = points[i].y;
    if( x < 0 || x >= AREA_W || y >= P4_OSD_ROWS * h ) continue;
    if( !was_down ) {
      tap_x = x;
      __sync_synchronize();
      tap_row = y / h;
    }
    down = true;
  }
  was_down = down;
}

int
p4_osd_tap( int *row, int *x )
{
  int r = tap_row;
  if( r < 0 ) return 0;
  __sync_synchronize();
  *row = r;
  *x = tap_x;
  tap_row = -1;
  return 1;
}

/* RGB565 a fifteenth of the way from bg to ink, for each coverage */
static void
shades( uint16_t bg, uint16_t ink, uint16_t out[ 16 ] )
{
  int r0 = bg >> 11, g0 = ( bg >> 5 ) & 63, b0 = bg & 31;
  int r1 = ink >> 11, g1 = ( ink >> 5 ) & 63, b1 = ink & 31;
  for( int a = 0; a < 16; a++ ) {
    out[a] = ( ( r0 + ( r1 - r0 ) * a / 15 ) << 11 ) |
             ( ( g0 + ( g1 - g0 ) * a / 15 ) << 5 ) |
             ( b0 + ( b1 - b0 ) * a / 15 );
  }
}

static void
draw_row( uint16_t *fb, int r, const char *text, int style, int h )
{
  const p4_osd_font_t *font = h > ROW_H_SHORT && !( style & P4_OSD_SMALL ) ?
                              &font_large : &font_small;
  style &= ~P4_OSD_SMALL;
  uint16_t bg = style == P4_OSD_INVERSE ? COLOUR_WHITE :
                style == P4_OSD_BUTTON ? COLOUR_GREY : COLOUR_BLACK;
  uint16_t ink = style == P4_OSD_INVERSE ? COLOUR_BLACK : COLOUR_WHITE;
  int y0 = r * h;
  int gap = h / 12;                     /* black below a button: 2 or 5 lines */
  int top = y0 + ( h - gap - font->height ) / 2;
  uint16_t shade[ 16 ];

  shades( bg, ink, shade );
  for( int y = 0; y < h; y++ ) {
    uint16_t *p = fb + ( y0 + y ) * P4_DISPLAY_WIDTH + AREA_X;
    /* a button keeps lines of black below it, so buttons stay apart */
    uint16_t c = style == P4_OSD_BUTTON && y >= h - gap ? COLOUR_BLACK : bg;
    for( int x = 0; x < AREA_W; x++ ) p[x] = c;
  }
  int x = MARGIN;
  for( int i = 0; i < P4_OSD_CHARS && text[i]; i++ ) {
    unsigned char ch = text[i];
    if( ch == '\t' ) {
      x = ( x / TAB + 1 ) * TAB;
      continue;
    }
    if( ch < P4_OSD_FONT_FIRST || ch > P4_OSD_FONT_LAST ) ch = '?';
    const p4_osd_glyph_t *g = &font->glyphs[ ch - P4_OSD_FONT_FIRST ];
    int gx = x + g->x;
    if( gx + g->w > AREA_W ) break;     /* off the end: cut there */
    const uint8_t *bits = font->data + g->offset;
    int stride = ( g->w + 1 ) / 2;
    for( int y = 0; y < g->h; y++ ) {
      uint16_t *p = fb + ( top + g->y + y ) * P4_DISPLAY_WIDTH + AREA_X + gx;
      for( int c = 0; c < g->w; c++ ) {
        int a = bits[ y * stride + c / 2 ] >> ( ( c & 1 ) * 4 ) & 15;
        if( a ) p[c] = shade[a];
      }
    }
    x += g->advance;
  }
  esp_cache_msync( fb + y0 * P4_DISPLAY_WIDTH + AREA_X,
                   ( ( h - 1 ) * P4_DISPLAY_WIDTH + AREA_W ) * 2,
                   ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED );
}

int
p4_osd_draw( uint16_t *fb, int buffer, int shown )
{
  int differs = 0;

  if( !ready ) return 0;
  int h = row_h;
  for( int r = 0; r < P4_OSD_ROWS; r++ ) {
    uint32_t v = rows[r].version;
    if( held[ buffer ][ r ] != v ) {
      char text[ P4_OSD_CHARS ];
      __sync_synchronize();
      memcpy( text, rows[r].text, P4_OSD_CHARS );
      int style = rows[r].style;
      draw_row( fb, r, text, style, h );
      /* changed while copied: leave it stale, to draw again next frame */
      held[ buffer ][ r ] = rows[r].version == v ? v : 0;
    }
    if( held[ shown ][ r ] != rows[r].version ) differs = 1;
  }
  return differs;
}
