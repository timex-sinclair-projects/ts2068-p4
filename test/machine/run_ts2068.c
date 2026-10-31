/* run_ts2068: run the machine on the host for N frames and write what
   fuse-tspico's automation writes, for tools/host/compare_fuse.py.

     run_ts2068 --rom0 HOME.rom --rom1 EXROM.rom --frames N
                [--fuse-contention] [--state state.json]
                [--screen screen.ppm] [--ram ram.bin] [--time] [--bench]

   --rom1 may be 8K or 16K. A 32K TS-Pico ROM can be given as --rom alone:
   its first half is HOME, its second the EXROM. No TS-Pico is attached,
   so ports 0Eh/0Fh read FFh, as in Fuse without --tspico. --bench types
   the benchmark's BASIC loop once the ROM has booted (basic_loop.c). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "basic_loop.h"
#include "machine.h"

static unsigned char *
load( const char *path, size_t *size )
{
  FILE *f = fopen( path, "rb" );
  unsigned char *buf;
  long n;

  if( !f ) { perror( path ); exit( 1 ); }
  fseek( f, 0, SEEK_END );
  n = ftell( f );
  fseek( f, 0, SEEK_SET );
  buf = malloc( n );
  if( fread( buf, 1, n, f ) != (size_t)n ) { perror( path ); exit( 1 ); }
  fclose( f );
  *size = n;
  return buf;
}

/* Fuse's spectrum-rgb palette, as its PNG screenshots have it */
static const unsigned char palette[ 16 ][ 3 ] = {
  { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xc0 }, { 0xc0, 0x00, 0x00 }, { 0xc0, 0x00, 0xc0 },
  { 0x00, 0xc0, 0x00 }, { 0x00, 0xc0, 0xc0 }, { 0xc0, 0xc0, 0x00 }, { 0xc0, 0xc0, 0xc0 },
  { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xff }, { 0xff, 0x00, 0x00 }, { 0xff, 0x00, 0xff },
  { 0x00, 0xff, 0x00 }, { 0x00, 0xff, 0xff }, { 0xff, 0xff, 0x00 }, { 0xff, 0xff, 0xff },
};

static void
write_screen( const char *path )
{
  static unsigned char pixels[ TS2068_SCREEN_WIDTH * TS2068_SCREEN_HEIGHT ];
  FILE *f = fopen( path, "wb" );
  size_t i;

  if( !f ) { perror( path ); exit( 1 ); }
  ts2068_render( pixels );
  fprintf( f, "P6\n%d %d\n255\n", TS2068_SCREEN_WIDTH, TS2068_SCREEN_HEIGHT );
  for( i = 0; i < sizeof( pixels ); i++ ) fwrite( palette[ pixels[ i ] ], 1, 3, f );
  fclose( f );
}

static void
write_state( const char *path, unsigned frames )
{
  ts2068_state_t s;
  FILE *f = fopen( path, "w" );

  if( !f ) { perror( path ); exit( 1 ); }
  ts2068_get_state( &s );
  fprintf( f, "{\"frames_completed\":%u,\"cpu\":{"
           "\"af\":%u,\"bc\":%u,\"de\":%u,\"hl\":%u,"
           "\"af_alt\":%u,\"bc_alt\":%u,\"de_alt\":%u,\"hl_alt\":%u,"
           "\"ix\":%u,\"iy\":%u,\"sp\":%u,\"pc\":%u,\"i\":%u,\"r\":%u,"
           "\"iff1\":%s,\"iff2\":%s,\"interrupt_mode\":%u,\"halted\":%s,"
           "\"frame_tstate\":%u},"
           "\"scld\":{\"dec\":%u,\"hsr\":%u,\"border\":%u}}\n",
           frames, s.af, s.bc, s.de, s.hl, s.af_, s.bc_, s.de_, s.hl_,
           s.ix, s.iy, s.sp, s.pc, s.i, s.r,
           s.iff1 ? "true" : "false", s.iff2 ? "true" : "false", s.im,
           s.halted ? "true" : "false", s.frame_tstate,
           s.dec, s.hsr, s.border );
  fclose( f );
}

int
main( int argc, char **argv )
{
  const char *rom = NULL, *rom0 = NULL, *rom1 = NULL;
  const char *state = NULL, *screen = NULL, *ramfile = NULL;
  unsigned frames = 100, i;
  int timed = 0, bench = 0;
  ts2068_config_t cfg = { 0 };
  size_t n0, n1;
  unsigned char *b0, *b1;
  clock_t t0;

  for( i = 1; i < (unsigned)argc; i++ ) {
    const char *a = argv[ i ], *v = i + 1 < (unsigned)argc ? argv[ i + 1 ] : NULL;
    if( !strcmp( a, "--rom" ) && v ) { rom = v; i++; }
    else if( !strcmp( a, "--rom0" ) && v ) { rom0 = v; i++; }
    else if( !strcmp( a, "--rom1" ) && v ) { rom1 = v; i++; }
    else if( !strcmp( a, "--frames" ) && v ) { frames = strtoul( v, NULL, 0 ); i++; }
    else if( !strcmp( a, "--state" ) && v ) { state = v; i++; }
    else if( !strcmp( a, "--screen" ) && v ) { screen = v; i++; }
    else if( !strcmp( a, "--ram" ) && v ) { ramfile = v; i++; }
    else if( !strcmp( a, "--fuse-contention" ) ) cfg.fuse_dock_contention = 1;
    else if( !strcmp( a, "--time" ) ) timed = 1;
    else if( !strcmp( a, "--bench" ) ) bench = 1;
    else { fprintf( stderr, "run_ts2068: bad argument %s\n", a ); return 2; }
  }

  if( rom ) {
    b0 = load( rom, &n0 );
    if( n0 != 0x8000 ) { fprintf( stderr, "%s: not 32K\n", rom ); return 1; }
    b1 = b0 + 0x4000; n1 = 0x4000; n0 = 0x4000;
  } else if( rom0 && rom1 ) {
    b0 = load( rom0, &n0 );
    b1 = load( rom1, &n1 );
  } else {
    fprintf( stderr, "run_ts2068: --rom, or --rom0 and --rom1\n" );
    return 2;
  }
  if( n0 != 0x4000 ) { fprintf( stderr, "HOME ROM: not 16K\n" ); return 1; }

  cfg.home_rom = b0;
  cfg.exrom = b1;
  cfg.exrom_size = n1;
  if( ts2068_init( &cfg ) ) { fprintf( stderr, "EXROM: not 8K or 16K\n" ); return 1; }

  t0 = clock();
  for( i = 0; i < frames; i++ ) {
    if( bench ) basic_loop_frame( i );
    ts2068_run_frame();
  }
  if( timed ) {
    double s = (double)( clock() - t0 ) / CLOCKS_PER_SEC;
    fprintf( stderr, "%u frames in %.3f s: %.1f us/frame, %.0fx real time\n",
             frames, s, s * 1e6 / frames, frames / 60.1145 / s );
  }

  if( state ) write_state( state, frames );
  if( screen ) write_screen( screen );
  if( ramfile ) {
    FILE *f = fopen( ramfile, "wb" );
    if( !f ) { perror( ramfile ); return 1; }
    fwrite( ts2068_home_ram(), 1, 0xc000, f );
    fclose( f );
  }
  return 0;
}
