/* zexrun: run a CP/M .com Z80 exerciser (zexdoc, zexall) on the lifted
   core, z80/.

     zexrun zex/zexdoc.com

   A flat 64K, no contention. The program is loaded at 0x0100. CP/M's two
   entry points are OUT instructions that this file traps:

     0x0000  OUT (0FEh),A      warm boot: the program has finished
     0x0005  OUT (0FFh),A; RET BDOS: C=2 prints E, C=9 prints from DE to '$'

   and (0x0006) holds the top of the TPA, which the exercisers load into SP.
   Exits 0 if the output contains no "ERROR". Built with -DCORETEST, so the
   contention calls are plain functions here, as in coretest.c. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z80_glue.h"
#include "z80_core.h"

libspectrum_dword tstates;
libspectrum_dword event_next_event;
libspectrum_dword z80_interrupt_length = 0;
int z80_int_disabled = 1;

static libspectrum_byte memory[ 0x10000 ];
static int finished, errors;
static char line[ 256 ];
static size_t line_len;

int event_register( event_fn_t fn, const char *d ) { (void)fn; (void)d; return 0; }
void event_add( libspectrum_dword t, int type ) { (void)t; (void)type; }

void contend_read( libspectrum_word a, libspectrum_dword t ) { (void)a; tstates += t; }
void contend_read_no_mreq( libspectrum_word a, libspectrum_dword t ) { (void)a; tstates += t; }
void contend_write_no_mreq( libspectrum_word a, libspectrum_dword t ) { (void)a; tstates += t; }

libspectrum_byte readbyte_internal( libspectrum_word a ) { return memory[ a ]; }
libspectrum_byte readbyte( libspectrum_word a ) { tstates += 3; return memory[ a ]; }
void writebyte( libspectrum_word a, libspectrum_byte b ) { tstates += 3; memory[ a ] = b; }

/* Echo the output, and count the lines that report a failure */
static void
put( char c )
{
  putchar( c );
  if( c == '\n' || line_len == sizeof( line ) - 1 ) {
    line[ line_len ] = 0;
    if( strstr( line, "ERROR" ) ) errors++;
    line_len = 0;
  } else if( c != '\r' ) {
    line[ line_len++ ] = c;
  }
  if( c == '\n' ) fflush( stdout );
}

libspectrum_byte readport( libspectrum_word port ) { (void)port; tstates += 4; return 0xff; }

void
writeport( libspectrum_word port, libspectrum_byte b )
{
  z80_regs_t r;

  (void)b;
  tstates += 4;
  z80_get_regs( &r );
  if( ( port & 0xff ) == 0xfe ) {
    finished = 1;
    event_next_event = 0;              /* leave z80_do_opcodes now */
  } else if( ( port & 0xff ) == 0xff ) {
    if( ( r.bc & 0xff ) == 2 ) {
      put( r.de & 0xff );
    } else if( ( r.bc & 0xff ) == 9 ) {
      libspectrum_word p;
      for( p = r.de; memory[ p ] != '$'; p++ ) put( memory[ p ] );
    }
  }
}

int
main( int argc, char **argv )
{
  FILE *f;
  size_t n;
  unsigned long long total = 0;
  z80_regs_t regs;

  if( argc != 2 ) { fprintf( stderr, "usage: %s program.com\n", argv[ 0 ] ); return 2; }
  f = fopen( argv[ 1 ], "rb" );
  if( !f ) { perror( argv[ 1 ] ); return 2; }
  n = fread( memory + 0x100, 1, 0x10000 - 0x100, f );
  fclose( f );
  if( !n ) { fprintf( stderr, "%s: empty\n", argv[ 1 ] ); return 2; }

  memory[ 0x0000 ] = 0xd3; memory[ 0x0001 ] = 0xfe;      /* OUT (0FEh),A */
  memory[ 0x0005 ] = 0xd3; memory[ 0x0006 ] = 0xff;      /* OUT (0FFh),A */
  memory[ 0x0007 ] = 0xc9;                               /* RET */
  /* (0x0006) is read as a word: 0xFF, 0xC9 = 0xC9FF, below which the
     stack grows. The exercisers are under 9K, so that's plenty. */

  z80_init( NULL );
  z80_reset( 1 );
  z80_get_regs( &regs );
  regs.pc = 0x0100;
  regs.sp = 0xc9ff;
  z80_set_regs( &regs );

  while( !finished ) {
    tstates = 0;
    event_next_event = 1000000000;
    z80_do_opcodes();
    total += tstates;
  }

  printf( "\nzexrun: %llu T-states, %d error line%s\n", total, errors,
          errors == 1 ? "" : "s" );
  return errors ? 1 : 0;
}
