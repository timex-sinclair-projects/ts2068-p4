/* coretest.c: Test program for Fuse's Z80 core
   Copyright (c) 2003-2017 Philip Kendall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z80_glue.h"
#include "z80_core.h"

static const char *progname;		/* argv[0] */
static const char *testsfile;		/* argv[1] */

libspectrum_dword tstates;
libspectrum_dword event_next_event;

/* 64Kb of RAM */
static libspectrum_byte initial_memory[ 0x10000 ], memory[ 0x10000 ];

libspectrum_byte readbyte( libspectrum_word address );
libspectrum_byte readbyte_internal( libspectrum_word address );

void writebyte( libspectrum_word address, libspectrum_byte b );
void writebyte_internal( libspectrum_word address, libspectrum_byte b );

static int run_test( FILE *f );
static int read_test( FILE *f, libspectrum_dword *end_tstates );

static void dump_z80_state( void );
static void dump_memory_state( void );

int
main( int argc, char **argv )
{
  FILE *f;

  progname = argv[0];

  if( argc < 2 ) {
    fprintf( stderr, "Usage: %s <testsfile>\n", progname );
    return 1;
  }

  testsfile = argv[1];

  /* Initialise the tables used by the Z80 core */
  z80_init( NULL );

  f = fopen( testsfile, "r" );
  if( !f ) {
    fprintf( stderr, "%s: couldn't open tests file `%s': %s\n", progname,
	     testsfile, strerror( errno ) );
    return 1;
  }

  while( run_test( f ) ) {
    /* Do nothing */
  }

  if( fclose( f ) ) {
    fprintf( stderr, "%s: couldn't close `%s': %s\n", progname, testsfile,
	     strerror( errno ) );
    return 1;
  }

  return 0;
}

libspectrum_byte
readbyte( libspectrum_word address )
{
  printf( "%5d MC %04x\n", tstates, address );
  tstates += 3;
  return readbyte_internal( address );
}

libspectrum_byte
readbyte_internal( libspectrum_word address )
{
  printf( "%5d MR %04x %02x\n", tstates, address, memory[ address ] );
  return memory[ address ];
}

void
writebyte( libspectrum_word address, libspectrum_byte b )
{
  printf( "%5d MC %04x\n", tstates, address );
  tstates += 3;
  writebyte_internal( address, b );
}

void
writebyte_internal( libspectrum_word address, libspectrum_byte b )
{
  printf( "%5d MW %04x %02x\n", tstates, address, b );
  memory[ address ] = b;
}

void
contend_read( libspectrum_word address, libspectrum_dword time )
{
  printf( "%5d MC %04x\n", tstates, address );
  tstates += time;
}

void
contend_read_no_mreq( libspectrum_word address, libspectrum_dword time )
{
  contend_read( address, time );
}

void
contend_write_no_mreq( libspectrum_word address, libspectrum_dword time )
{
  printf( "%5d MC %04x\n", tstates, address );
  tstates += time;
}

static void
contend_port_preio( libspectrum_word port )
{
  if( ( port & 0xc000 ) == 0x4000 ) {
    printf( "%5d PC %04x\n", tstates, port );
  }

  tstates++;
}

static void
contend_port_postio( libspectrum_word port )
{
  if( port & 0x0001 ) {
    
    if( ( port & 0xc000 ) == 0x4000 ) {
      printf( "%5d PC %04x\n", tstates, port ); tstates++;
      printf( "%5d PC %04x\n", tstates, port ); tstates++;
      printf( "%5d PC %04x\n", tstates, port ); tstates++;
    } else {
      tstates += 3;
    }

  } else {

    printf( "%5d PC %04x\n", tstates, port ); tstates += 3;

  }
}

libspectrum_byte
readport( libspectrum_word port )
{
  libspectrum_byte r = port >> 8;

  contend_port_preio( port );

  printf( "%5d PR %04x %02x\n", tstates, port, r );

  contend_port_postio( port );

  return r;
}

void
writeport( libspectrum_word port, libspectrum_byte b )
{
  contend_port_preio( port );

  printf( "%5d PW %04x %02x\n", tstates, port, b );

  contend_port_postio( port );
}

static int
run_test( FILE *f )
{
  size_t i;

  /* Get ourselves into a known state */
  z80_reset( 1 ); tstates = 0;
  for( i = 0; i < 0x10000; i += 4 ) {
    memory[ i     ] = 0xde; memory[ i + 1 ] = 0xad;
    memory[ i + 2 ] = 0xbe; memory[ i + 3 ] = 0xef;
  }

  if( read_test( f, &event_next_event ) ) return 0;

  /* Grab a copy of the memory for comparison at the end */
  memcpy( initial_memory, memory, 0x10000 );

  z80_do_opcodes();

  /* And dump our final state */
  dump_z80_state();
  dump_memory_state();

  printf( "\n" );

  return 1;
}

static int
read_test( FILE *f, libspectrum_dword *end_tstates )
{
  unsigned af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc, memptr;
  unsigned i, r, iff1, iff2, im, halted;
  z80_regs_t regs = { 0 };
  unsigned end_tstates2;
  unsigned address;
  char test_name[ 80 ];

  do {

    if( !fgets( test_name, sizeof( test_name ), f ) ) {

      if( feof( f ) ) return 1;

      fprintf( stderr, "%s: error reading test description from `%s': %s\n",
	       progname, testsfile, strerror( errno ) );
      return 1;
    }

  } while( test_name[0] == '\n' );

  /* FIXME: how should we read/write our data types? */
  if( fscanf( f, "%x %x %x %x %x %x %x %x %x %x %x %x %x", &af, &bc,
	      &de, &hl, &af_, &bc_, &de_, &hl_, &ix, &iy, &sp, &pc,
	      &memptr ) != 13 ) {
    fprintf( stderr, "%s: first registers line in `%s' corrupt\n", progname,
	     testsfile );
    return 1;
  }

  regs.af  = af;  regs.bc  = bc;  regs.de  = de;  regs.hl  = hl;
  regs.af_ = af_; regs.bc_ = bc_; regs.de_ = de_; regs.hl_ = hl_;
  regs.ix  = ix;  regs.iy  = iy;  regs.sp  = sp;  regs.pc  = pc;
  regs.memptr = memptr;

  if( fscanf( f, "%x %x %u %u %u %u %u", &i, &r, &iff1, &iff2, &im,
	      &halted, &end_tstates2 ) != 7 ) {
    fprintf( stderr, "%s: second registers line in `%s' corrupt\n", progname,
	     testsfile );
    return 1;
  }

  regs.i = i; regs.r = r; regs.iff1 = iff1; regs.iff2 = iff2; regs.im = im;
  regs.halted = halted;
  z80_set_regs( &regs );
  *end_tstates = end_tstates2;

  while( 1 ) {

    if( fscanf( f, "%x", &address ) != 1 ) {
      fprintf( stderr, "%s: no address found in `%s'\n", progname, testsfile );
      return 1;
    }

    if( address >= 0x10000 ) break;

    while( 1 ) {

      unsigned byte;

      if( fscanf( f, "%x", &byte ) != 1 ) {
	fprintf( stderr, "%s: no data byte found in `%s'\n", progname,
		 testsfile );
	return 1;
      }
    
      if( byte >= 0x100 ) break;

      memory[ address++ ] = byte;

    }
  }

  printf( "%s", test_name );

  return 0;
}

static void
dump_z80_state( void )
{
  z80_regs_t r;

  z80_get_regs( &r );
  printf( "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
	  r.af, r.bc, r.de, r.hl, r.af_, r.bc_, r.de_, r.hl_, r.ix, r.iy, r.sp,
	  r.pc, r.memptr );
  printf( "%02x %02x %d %d %d %d %d\n", r.i, r.r,
	  r.iff1, r.iff2, r.im, r.halted, tstates );
}

static void
dump_memory_state( void )
{
  size_t i;

  for( i = 0; i < 0x10000; i++ ) {

    if( memory[ i ] == initial_memory[ i ] ) continue;

    printf( "%04x ", (unsigned)i );

    while( i < 0x10000 && memory[ i ] != initial_memory[ i ] )
      printf( "%02x ", memory[ i++ ] );

    printf( "-1\n" );
  }
}

/* What the core needs from a machine (z80/z80_glue.h): no events fire
   during a test, and no interrupt is ever accepted. */

libspectrum_dword z80_interrupt_length = 0;
int z80_int_disabled = 1;

int
event_register( event_fn_t fn GCC_UNUSED, const char *description GCC_UNUSED )
{
  static int next;
  return next++;
}

void
event_add( libspectrum_dword event_time GCC_UNUSED, int type GCC_UNUSED )
{
  /* Do nothing */
}
