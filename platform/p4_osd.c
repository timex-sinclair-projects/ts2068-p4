/* p4_osd.c: the side panel's menu area (p4_osd.h).

   Each row carries a version, bumped when its text changes; each frame
   buffer remembers the version of each row it holds, and the draw brings
   the rows that differ up to date, as p4_keyboard.c does for keys. */

#include <stdbool.h>
#include <string.h>

#include "esp_cache.h"

#include "p4_display.h"
#include "p4_osd.h"

#define AREA_X  ( P4_DISPLAY_WIDTH - 2 - 380 )  /* lined up with the keyboard */
#define AREA_W  380
#define ROW_H   24
#define AREA_H  ( P4_OSD_ROWS * ROW_H )
#define GLYPH_W 6                               /* the font's columns 1-6 */
#define SCALE   2
#define CHAR_W  ( GLYPH_W * SCALE )
#define TEXT_X  ( AREA_X + ( AREA_W - P4_OSD_COLS * CHAR_W ) / 2 )

#define COLOUR_BLACK 0x0000
#define COLOUR_GREY  0x3186                     /* the keys' face */
#define COLOUR_WHITE 0xffff

static uint8_t font[ 96 * 8 ];
static bool ready;
static struct {
  char text[ P4_OSD_COLS ];
  uint8_t style;
  volatile uint32_t version;
} rows[ P4_OSD_ROWS ];
static uint32_t held[ P4_DISPLAY_BUFFERS ][ P4_OSD_ROWS ];
static bool was_down;
static volatile int tap_row = -1, tap_col;

void
p4_osd_init( const uint8_t *f )
{
  memcpy( font, f, sizeof( font ) );
  memset( held, 0, sizeof( held ) );    /* versions start at 1: all redrawn */
  for( int r = 0; r < P4_OSD_ROWS; r++ ) {
    if( !rows[r].version ) memset( rows[r].text, ' ', P4_OSD_COLS );
    rows[r].version++;
  }
  tap_row = -1;
  ready = true;
}

void
p4_osd_text( int row, const char *text, int style )
{
  if( row < 0 || row >= P4_OSD_ROWS ) return;
  int n = strnlen( text, P4_OSD_COLS );
  memcpy( rows[ row ].text, text, n );
  memset( rows[ row ].text + n, ' ', P4_OSD_COLS - n );
  rows[ row ].style = style;
  __sync_synchronize();                 /* the text before the version */
  rows[ row ].version++;
}

void
p4_osd_touch( const p4_touch_point_t *points, int count )
{
  bool down = false;
  for( int i = 0; i < count; i++ ) {
    int x = points[i].x - AREA_X, y = points[i].y;
    if( x < 0 || x >= AREA_W || y >= AREA_H ) continue;
    if( !was_down ) {
      int col = ( points[i].x - TEXT_X ) / CHAR_W;
      tap_col = col < 0 ? 0 : col >= P4_OSD_COLS ? P4_OSD_COLS - 1 : col;
      __sync_synchronize();
      tap_row = y / ROW_H;
    }
    down = true;
  }
  was_down = down;
}

int
p4_osd_tap( int *row, int *col )
{
  int r = tap_row;
  if( r < 0 ) return 0;
  __sync_synchronize();
  *row = r;
  *col = tap_col;
  tap_row = -1;
  return 1;
}

static void
draw_row( uint16_t *fb, int r, const char *text, int style )
{
  uint16_t bg = style == P4_OSD_INVERSE ? COLOUR_WHITE :
                style == P4_OSD_BUTTON ? COLOUR_GREY : COLOUR_BLACK;
  uint16_t ink = style == P4_OSD_INVERSE ? COLOUR_BLACK : COLOUR_WHITE;
  int y0 = r * ROW_H;

  for( int y = 0; y < ROW_H; y++ ) {
    uint16_t *p = fb + ( y0 + y ) * P4_DISPLAY_WIDTH + AREA_X;
    /* a button keeps a line of black below it, so buttons stay apart */
    uint16_t c = style == P4_OSD_BUTTON && y >= ROW_H - 2 ? COLOUR_BLACK : bg;
    for( int x = 0; x < AREA_W; x++ ) p[x] = c;
  }
  for( int i = 0; i < P4_OSD_COLS; i++ ) {
    char ch = text[i];
    if( ch <= ' ' || ch > 0x7f ) continue;
    const uint8_t *g = font + ( ch - ' ' ) * 8;
    for( int row = 0; row < 8 * SCALE; row++ ) {
      uint8_t bits = g[ row / SCALE ];
      uint16_t *p = fb + ( y0 + 4 + row ) * P4_DISPLAY_WIDTH + TEXT_X + i * CHAR_W;
      for( int col = 0; col < CHAR_W; col++ ) {
        if( bits & ( 0x40 >> ( col / SCALE ) ) ) p[col] = ink;
      }
    }
  }
  esp_cache_msync( fb + y0 * P4_DISPLAY_WIDTH + AREA_X,
                   ( ( ROW_H - 1 ) * P4_DISPLAY_WIDTH + AREA_W ) * 2,
                   ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED );
}

int
p4_osd_draw( uint16_t *fb, int buffer, int shown )
{
  int differs = 0;

  if( !ready ) return 0;
  for( int r = 0; r < P4_OSD_ROWS; r++ ) {
    uint32_t v = rows[r].version;
    if( held[ buffer ][ r ] != v ) {
      char text[ P4_OSD_COLS ];
      __sync_synchronize();
      memcpy( text, rows[r].text, P4_OSD_COLS );
      int style = rows[r].style;
      draw_row( fb, r, text, style );
      /* changed while copied: leave it stale, to draw again next frame */
      held[ buffer ][ r ] = rows[r].version == v ? v : 0;
    }
    if( held[ shown ][ r ] != rows[r].version ) differs = 1;
  }
  return differs;
}
