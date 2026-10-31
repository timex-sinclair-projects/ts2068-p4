/* szx.c: SZX (zx-state) snapshots of the TS2068 (machine.h).

   The bytes are libspectrum's (szx.c, 1.5.0): written as libspectrum
   writes a TS2068, chunk for chunk, so that libspectrum reading ours and
   writing it back uncompressed gives the same file; read as libspectrum
   reads them, so that Fuse's snapshots load. No code is copied from it.

     header   "ZXST", version 1.5, machine 12 (TS2068), flags 0
     Z80R     the Z80 (37 bytes)
     SPCR     port FE (8)
     JOY      Timex joysticks 1 and 2 (6)
     KEYB     no issue 2, no keyboard joystick (5)
     RAMP     pages 5, 2, 0: HOME RAM 0x4000, 0x8000, 0xC000 (3 + 16K each)
     AY       the selected register and the 16 registers (18)
     SCLD     ports F4 and FF (2)
     ZXPR     no ZX Printer (2)

   We write the pages uncompressed, so saving needs no deflate. Loading
   inflates zlib-compressed pages, which libspectrum writes by default:
   zlib on the host, the ROM's miniz on the P4 (SZX_ROM_MINIZ, which the P4
   build defines). Chunks this machine has
   no use for are skipped; a DOCK-bank page is refused, since there is no
   DOCK, and EXROM-bank pages are skipped, since the EXROM comes from the
   machine's configuration (Fuse writes them only with a DOCK). */

#include <string.h>

/* MicroPython's qstr scan preprocesses this without the module's own
   definitions, so no zlib to include means the P4 too */
#if !defined( SZX_ROM_MINIZ ) && !__has_include( <zlib.h> )
#define SZX_ROM_MINIZ
#endif

#ifdef SZX_ROM_MINIZ
#include "miniz.h"                      /* esp_rom: tinfl, in the P4's ROM */
#else
#include <zlib.h>
#endif

#include "machine.h"

#define MACHINE_TS2068 12
#define VERSION_MAJOR 1
#define VERSION_MINOR 5

#define Z80R_EILAST 1
#define Z80R_HALTED 2
#define Z80R_FSET   4
#define RAMP_COMPRESSED 1
#define DOCK_EXROMDOCK  4               /* set: the DOCK bank; clear: EXROM */
#define JOY_TIMEX1 6
#define JOY_TIMEX2 7
#define JOY_NONE   8

#define PAGE 0x4000

/* SZX page number -> offset in HOME RAM (0x4000-0xFFFF) */
static const struct { uint8_t page; uint16_t offset; } pages[3] = {
  { 5, 0x0000 }, { 2, 0x4000 }, { 0, 0x8000 },
};

static uint8_t *
put16( uint8_t *p, uint16_t v )
{
  p[0] = v; p[1] = v >> 8;
  return p + 2;
}

static uint8_t *
put32( uint8_t *p, uint32_t v )
{
  p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
  return p + 4;
}

static uint16_t get16( const uint8_t *p ) { return p[0] | p[1] << 8; }
static uint32_t get32( const uint8_t *p ) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static uint8_t *
chunk( uint8_t *p, const char *id, uint32_t length )
{
  memcpy( p, id, 4 );
  return put32( p + 4, length );
}

size_t
ts2068_szx_save( uint8_t *buf, size_t size )
{
  ts2068_snap_t s;
  uint8_t *p = buf, flags = 0;
  int i;

  if( size < TS2068_SZX_SIZE ) return 0;
  ts2068_snap_get( &s );

  memcpy( p, "ZXST", 4 );
  p[4] = VERSION_MAJOR; p[5] = VERSION_MINOR; p[6] = MACHINE_TS2068; p[7] = 0;
  p += 8;

  p = chunk( p, "Z80R", 37 );
  *p++ = s.af; *p++ = s.af >> 8;
  p = put16( p, s.bc ); p = put16( p, s.de ); p = put16( p, s.hl );
  *p++ = s.af_; *p++ = s.af_ >> 8;
  p = put16( p, s.bc_ ); p = put16( p, s.de_ ); p = put16( p, s.hl_ );
  p = put16( p, s.ix ); p = put16( p, s.iy );
  p = put16( p, s.sp ); p = put16( p, s.pc );
  *p++ = s.i; *p++ = s.r;
  *p++ = s.iff1; *p++ = s.iff2; *p++ = s.im;
  p = put32( p, s.tstates );
  /* libspectrum's, with Fuse's 48 T-states, not the TS2068's 32 */
  *p++ = s.tstates < 48 ? 48 - s.tstates : 0;
  if( s.ei_last ) flags |= Z80R_EILAST;
  if( s.halted ) flags |= Z80R_HALTED;
  if( s.f_set ) flags |= Z80R_FSET;
  *p++ = flags;
  p = put16( p, s.memptr );

  p = chunk( p, "SPCR", 8 );
  *p++ = s.ula & 0x07; *p++ = 0; *p++ = 0; *p++ = s.ula;
  p = put32( p, 0 );

  p = chunk( p, "JOY\0", 6 );
  p = put32( p, 0 );
  *p++ = JOY_TIMEX1; *p++ = JOY_TIMEX2;

  p = chunk( p, "KEYB", 5 );
  p = put32( p, 0 );
  *p++ = JOY_NONE;

  for( i = 0; i < 3; i++ ) {
    p = chunk( p, "RAMP", 3 + PAGE );
    p = put16( p, 0 );
    *p++ = pages[i].page;
    memcpy( p, ts2068_home_ram() + pages[i].offset, PAGE );
    p += PAGE;
  }

  p = chunk( p, "AY\0\0", 18 );
  *p++ = 0; *p++ = s.ay_register;
  memcpy( p, s.ay, 16 ); p += 16;

  p = chunk( p, "SCLD", 2 );
  *p++ = s.hsr; *p++ = s.dec;

  p = chunk( p, "ZXPR", 2 );
  p = put16( p, 0 );

  return p - buf;
}

/* Exactly PAGE bytes from a zlib stream */
static int
inflate_page( uint8_t *dst, const uint8_t *src, size_t len )
{
#ifdef SZX_ROM_MINIZ
  static tinfl_decompressor d;          /* 11K: not on a task's stack */
  size_t in = len, out = PAGE;
  tinfl_init( &d );
  tinfl_status st = tinfl_decompress( &d, src, &in, dst, dst, &out,
                                      TINFL_FLAG_PARSE_ZLIB_HEADER |
                                      TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF );
  return st == TINFL_STATUS_DONE && out == PAGE ? 0 : -1;
#else
  uLongf out = PAGE;
  return uncompress( dst, &out, src, len ) == Z_OK && out == PAGE ? 0 : -1;
#endif
}

/* A RAMP chunk's page into HOME RAM, if it's one of ours */
static int
load_page( const uint8_t *d, uint32_t len )
{
  int i;

  if( len < 3 ) return TS2068_SZX_CORRUPT;
  uint16_t flags = get16( d );
  uint8_t page = d[2];
  if( page > 63 ) return TS2068_SZX_CORRUPT;
  for( i = 0; i < 3 && pages[i].page != page; i++ ) ;
  if( i == 3 ) return 0;                /* never mapped on a TS2068 */
  uint8_t *dst = ts2068_home_ram() + pages[i].offset;
  if( flags & RAMP_COMPRESSED )
    return inflate_page( dst, d + 3, len - 3 ) ? TS2068_SZX_INFLATE : 0;
  if( len != 3 + PAGE ) return TS2068_SZX_CORRUPT;
  memcpy( dst, d + 3, PAGE );
  return 0;
}

int
ts2068_szx_load( const uint8_t *buf, size_t size )
{
  ts2068_snap_t s;
  const uint8_t *p = buf, *end = buf + size;
  int version, err, pass;

  if( size < 8 || memcmp( buf, "ZXST", 4 ) || buf[6] != MACHINE_TS2068 )
    return TS2068_SZX_NOT_SZX;
  version = buf[4] << 8 | buf[5];
  if( version < 0x0101 ) return TS2068_SZX_UNSUPPORTED;

  /* Missing chunks leave what libspectrum's snap_alloc leaves: zeros */
  memset( &s, 0, sizeof( s ) );

  /* Check every chunk before touching the machine, then load the pages */
  for( pass = 0; pass < 2; pass++ ) {
    for( p = buf + 8; p < end; ) {
      if( end - p < 8 ) return TS2068_SZX_CORRUPT;
      const uint8_t *d = p + 8;
      uint32_t len = get32( p + 4 );
      if( len > (size_t)( end - d ) ) return TS2068_SZX_CORRUPT;

      if( !memcmp( p, "RAMP", 4 ) ) {
        if( len < 3 || d[2] > 63 ) return TS2068_SZX_CORRUPT;
        if( pass && ( err = load_page( d, len ) ) ) return err;
      } else if( pass ) {
        /* the rest were taken on the first pass */
      } else if( !memcmp( p, "Z80R", 4 ) ) {
        if( len != 37 ) return TS2068_SZX_CORRUPT;
        s.af = d[0] | d[1] << 8; s.bc = get16( d + 2 ); s.de = get16( d + 4 );
        s.hl = get16( d + 6 ); s.af_ = d[8] | d[9] << 8; s.bc_ = get16( d + 10 );
        s.de_ = get16( d + 12 ); s.hl_ = get16( d + 14 ); s.ix = get16( d + 16 );
        s.iy = get16( d + 18 ); s.sp = get16( d + 20 ); s.pc = get16( d + 22 );
        s.i = d[24]; s.r = d[25]; s.iff1 = d[26]; s.iff2 = d[27]; s.im = d[28];
        s.tstates = get32( d + 29 );
        s.ei_last = !!( d[34] & Z80R_EILAST );
        s.halted = !!( d[34] & Z80R_HALTED );
        s.f_set = !!( d[34] & Z80R_FSET );
        s.memptr = version >= 0x0104 ? get16( d + 35 ) : 0;
      } else if( !memcmp( p, "SPCR", 4 ) ) {
        if( len != 8 ) return TS2068_SZX_CORRUPT;
        s.ula = ( d[0] & 0x07 ) | ( d[3] & 0xf8 );
      } else if( !memcmp( p, "AY\0\0", 4 ) ) {
        if( len != 18 ) return TS2068_SZX_CORRUPT;
        s.ay_register = d[1];
        memcpy( s.ay, d + 2, 16 );
      } else if( !memcmp( p, "SCLD", 4 ) ) {
        if( len != 2 ) return TS2068_SZX_CORRUPT;
        s.hsr = d[0]; s.dec = d[1];
      } else if( !memcmp( p, "DOCK", 4 ) ) {
        if( len < 3 || d[2] > 7 ) return TS2068_SZX_CORRUPT;
        if( get16( d ) & DOCK_EXROMDOCK ) return TS2068_SZX_UNSUPPORTED;
      }
      p = d + len;
    }
    /* Between the passes: the machine, then its RAM on the second */
    if( !pass ) ts2068_snap_set( &s );
  }
  return 0;
}
