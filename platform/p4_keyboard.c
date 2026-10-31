/* p4_keyboard.c: the on-screen TS2068 keyboard (p4_keyboard.h).

   The matrix: port FE's half-rows A8..A15 read the left halves of the
   rows from the bottom up (CAPS SHIFT..V, A..G, Q..T, 1..5) and then the
   right halves from the top down (0..6, P..Y, ENTER..H, SPACE..B), with
   bit 0 the key nearest the outside edge. */

#include <stdbool.h>
#include <string.h>

#include "esp_cache.h"

#include "p4_display.h"
#include "p4_keyboard.h"

#define COLS    10
#define ROWS    4
#define KEYS    ( COLS * ROWS )
#define PITCH_X 23                       /* 230: the side panel, less 2 */
#define PITCH_Y 90
#define KB_X    ( P4_DISPLAY_WIDTH - 2 - COLS * PITCH_X )
#define KB_Y    ( P4_DISPLAY_HEIGHT - ROWS * PITCH_Y )
#define GAP     2                       /* each side of a key */
#define GLYPH_W 6                       /* the font's columns 1-6 */

#define COLOUR_FACE  0x3186             /* dark grey */
#define COLOUR_LABEL 0xffff

/* Main label (drawn twice size), then the smaller one under it, if any:
   three letters at most, for 19-pixel keys */
static const char *const labels[ KEYS ][2] = {
  { "1" }, { "2" }, { "3" }, { "4" }, { "5" },
  { "6" }, { "7" }, { "8" }, { "9" }, { "0" },
  { "Q" }, { "W" }, { "E" }, { "R" }, { "T" },
  { "Y" }, { "U" }, { "I" }, { "O" }, { "P" },
  { "A" }, { "S" }, { "D" }, { "F" }, { "G" },
  { "H" }, { "J" }, { "K" }, { "L" }, { NULL, "ENT" },
  { NULL, "CAP\nSHF" }, { "Z" }, { "X" }, { "C" }, { "V" },
  { "B" }, { "N" }, { "M" }, { NULL, "SYM\nSHF" }, { NULL, "BRK\nSPC" },
};

static uint8_t font[ 96 * 8 ];
static bool ready;
static uint64_t down;
static uint64_t held[ P4_DISPLAY_BUFFERS ];   /* the keys each buffer shows down */
static uint8_t drawn;                         /* buffers with the keyboard in */
static volatile bool shown_on = true;
static volatile uint32_t layout = 1;          /* bumped when shown or hidden */
static uint32_t laid[ P4_DISPLAY_BUFFERS ];   /* the layout each buffer has */

void
p4_keyboard_init( const uint8_t *f )
{
  memcpy( font, f, sizeof( font ) );
  down = 0;
  drawn = 0;
  layout++;
  ready = true;
}

void
p4_keyboard_show( int show )
{
  bool on = show != 0;
  if( on == shown_on ) return;
  shown_on = on;
  __sync_synchronize();
  layout++;
}

static void
key_matrix( int key, int *half_row, int *bit )
{
  int r = key / COLS, c = key % COLS;
  if( c < 5 ) {
    *half_row = 3 - r;
    *bit = c;
  } else {
    *half_row = 4 + r;
    *bit = 9 - c;
  }
}

void
p4_keyboard_touch( const p4_touch_point_t *points, int count, uint8_t rows[8] )
{
  uint64_t d = 0;
  for( int i = 0; i < count && shown_on; i++ ) {
    int x = points[i].x - KB_X, y = points[i].y - KB_Y;
    if( x < 0 || y < 0 || x >= COLS * PITCH_X || y >= ROWS * PITCH_Y ) continue;
    d |= 1ULL << ( y / PITCH_Y * COLS + x / PITCH_X );
  }
  down = d;
  memset( rows, 0xff, 8 );
  for( int key = 0; key < KEYS; key++ ) {
    if( !( d >> key & 1 ) ) continue;
    int half_row, bit;
    key_matrix( key, &half_row, &bit );
    rows[ half_row ] &= ~( 1 << bit );
  }
}

uint64_t
p4_keyboard_down( void )
{
  return down;
}

/* One character at (x, y), scale 1 or 2, columns 1-6 of the 8 */
static void
draw_char( uint16_t *fb, int x, int y, char ch, int scale, uint16_t colour )
{
  if( ch < ' ' || ch > 0x7f ) return;
  const uint8_t *g = font + ( ch - ' ' ) * 8;
  for( int row = 0; row < 8 * scale; row++ ) {
    uint8_t bits = g[ row / scale ];
    uint16_t *p = fb + ( y + row ) * P4_DISPLAY_WIDTH + x;
    for( int col = 0; col < GLYPH_W * scale; col++ ) {
      if( bits & ( 0x40 >> ( col / scale ) ) ) p[col] = colour;
    }
  }
}

/* Lines of text, centred on (cx, y) */
static void
draw_text( uint16_t *fb, int cx, int y, const char *s, int scale,
           uint16_t colour )
{
  while( *s ) {
    int n = strcspn( s, "\n" );
    int x = cx - n * GLYPH_W * scale / 2;
    for( int i = 0; i < n; i++ ) draw_char( fb, x + i * GLYPH_W * scale, y, s[i], scale, colour );
    s += n;
    if( *s ) s++;
    y += 8 * scale + 2;
  }
}

static void
draw_key( uint16_t *fb, int key, bool is_down )
{
  int x0 = KB_X + key % COLS * PITCH_X + GAP, y0 = KB_Y + key / COLS * PITCH_Y + GAP;
  int w = PITCH_X - 2 * GAP, h = PITCH_Y - 2 * GAP;
  uint16_t face = is_down ? COLOUR_LABEL : COLOUR_FACE;
  uint16_t ink = is_down ? COLOUR_FACE : COLOUR_LABEL;

  for( int y = 0; y < h; y++ ) {
    uint16_t *p = fb + ( y0 + y ) * P4_DISPLAY_WIDTH + x0;
    for( int x = 0; x < w; x++ ) p[x] = face;
  }
  const char *big = labels[ key ][0], *small = labels[ key ][1];
  int cx = x0 + w / 2;
  if( big ) draw_text( fb, cx, y0 + 12, big, 2, ink );
  if( small ) draw_text( fb, cx, y0 + ( big ? 40 : h / 2 - 9 ), small, 1, ink );
}

/* Write lines y0..y0+h of the side panel back from the cache, for the
   panel's DMA */
static void
sync_lines( uint16_t *fb, int y0, int h )
{
  uint16_t *start = fb + y0 * P4_DISPLAY_WIDTH + KB_X;
  size_t bytes = ( ( h - 1 ) * P4_DISPLAY_WIDTH + COLS * PITCH_X ) * 2;
  esp_cache_msync( start, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                                 ESP_CACHE_MSYNC_FLAG_UNALIGNED );
}

int
p4_keyboard_draw( uint16_t *fb, int buffer, int shown )
{
  if( !ready ) return 0;
  uint8_t bit = 1 << buffer;
  uint32_t l = layout;
  if( laid[ buffer ] != l ) {
    laid[ buffer ] = l;
    drawn &= ~bit;
  }
  if( !shown_on ) {
    /* the menu has the panel: p4_osd_draw paints over this buffer */
    return 0;
  }
  if( !( drawn & bit ) ) {
    /* black first: the gaps between keys may hold the tall menu */
    for( int y = KB_Y; y < P4_DISPLAY_HEIGHT; y++ ) {
      uint16_t *p = fb + y * P4_DISPLAY_WIDTH + KB_X;
      for( int x = 0; x < COLS * PITCH_X; x++ ) p[x] = 0;
    }
    for( int key = 0; key < KEYS; key++ ) draw_key( fb, key, down >> key & 1 );
    sync_lines( fb, KB_Y, ROWS * PITCH_Y );
    drawn |= bit;
    held[ buffer ] = down;
  } else if( held[ buffer ] != down ) {
    uint64_t changed = held[ buffer ] ^ down;
    for( int key = 0; key < KEYS; key++ ) {
      if( !( changed >> key & 1 ) ) continue;
      draw_key( fb, key, down >> key & 1 );
      sync_lines( fb, KB_Y + key / COLS * PITCH_Y, PITCH_Y );
    }
    held[ buffer ] = down;
  }
  return laid[ shown ] != l || !( drawn & ( 1 << shown ) ) || held[ shown ] != down;
}
