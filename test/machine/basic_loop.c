/* basic_loop.c: the fixed BASIC loop for the P4 benchmark (PLAN.md
   Phase 2): after the ROM has booted, type

     10 FOR I=1 TO 9999: NEXT I
     RUN

   on the emulated keyboard, one key every few frames, as a person would.
   Shared by test/machine/run_ts2068 (--bench) and the P4's ts2068 module. */

#include <string.h>

#include "basic_loop.h"
#include "machine.h"

/* The TS2068's keyboard is the Spectrum's: half-row (port FE's A8..A15)
   and bit. A key's chord is up to two of these. */
#define K( row, bit ) ( ( row ) << 3 | ( bit ) )
#define NONE 0xff
#define CAPS K( 0, 0 )
#define SYM  K( 7, 1 )

static const unsigned char keys[][ 2 ] = {
  { K( 3, 0 ), NONE },          /* 1 */
  { K( 4, 0 ), NONE },          /* 0 */
  { K( 1, 3 ), NONE },          /* F: FOR (K mode) */
  { K( 5, 2 ), NONE },          /* I */
  { SYM, K( 6, 1 ) },           /* symbol shift L: = */
  { K( 3, 0 ), NONE },          /* 1 */
  { SYM, K( 1, 3 ) },           /* symbol shift F: TO */
  { K( 4, 1 ), NONE },          /* 9 */
  { K( 4, 1 ), NONE },          /* 9 */
  { K( 4, 1 ), NONE },          /* 9 */
  { K( 4, 1 ), NONE },          /* 9 */
  { SYM, K( 0, 1 ) },           /* symbol shift Z: ":" */
  { K( 7, 3 ), NONE },          /* N: NEXT (K mode) */
  { K( 5, 2 ), NONE },          /* I */
  { K( 6, 0 ), NONE },          /* ENTER */
  { K( 2, 3 ), NONE },          /* R: RUN (K mode) */
  { K( 6, 0 ), NONE },          /* ENTER */
};

#define N_KEYS  ( sizeof( keys ) / sizeof( keys[ 0 ] ) )
#define HOLD    4               /* frames a key is down */
#define GAP     4               /* frames between keys */

void
basic_loop_frame( unsigned frame )
{
  unsigned char rows[ 8 ];
  unsigned n, phase;
  int i;

  memset( rows, 0xff, sizeof( rows ) );

  if( frame >= BASIC_LOOP_BOOT_FRAMES ) {
    n = ( frame - BASIC_LOOP_BOOT_FRAMES ) / ( HOLD + GAP );
    phase = ( frame - BASIC_LOOP_BOOT_FRAMES ) % ( HOLD + GAP );
    if( n < N_KEYS && phase < HOLD )
      for( i = 0; i < 2; i++ )
        if( keys[ n ][ i ] != NONE )
          rows[ keys[ n ][ i ] >> 3 ] &= ~( 1 << ( keys[ n ][ i ] & 7 ) );
  }

  ts2068_set_keyboard( rows );
}
