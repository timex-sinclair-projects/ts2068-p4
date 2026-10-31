/* ts2068.c: the Timex/Sinclair 2068 behind machine.h

   Derived from Fuse (third_party/fuse-tspico, tag tspico-v1.10.0-1):
   machines/ts2068.c and tc2068.c, peripherals/scld.c, ula.c and
   sound/ay.c, machines/machines_periph.c, spectrum.c, machine.c, periph.c
   and memory_pages.c.

   Copyright (c) 1999-2021 Philip Kendall, Fredrick Meunier, Witold Filipczyk,
                           Darren Salt
   Copyright (c) 2015 Stuart Brady, Adrien Destugues
   Copyright (c) 2026 the esp32-2068 contributors

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

   What Fuse spreads over its module, periph and memory_page systems is
   here in one file, cut to what the TS2068 has: 8K chunks paged by the
   SCLD (port F4, and port FF bit 7), the ULA on port FE, the SCLD's
   screen modes on port FF, the AY on F5/F6 with the joysticks on R14,
   and the TS-Pico on 0E/0F. Behaviour is Fuse's except where marked
   "Not Fuse". machine/README.md lists what is left out. */

#include <stdlib.h>
#include <string.h>

#include "machine.h"
#include "event.h"
#include "sound/sound.h"
#include "z80_core.h"
#include "z80_glue.h"

/* Frame timings: libspectrum's timings_frame_timex_scld_60hz */
#define LEFT_BORDER        24   /* T-states */
#define HORIZONTAL_SCREEN  128
#define INTERRUPT_LENGTH   32
#define TOP_LEFT_PIXEL     9169 /* T-states after /INT */

/* What Fuse shows of the border: 24 lines and 4 columns of 8 pixels */
#define BORDER_LINES       24
#define BORDER_COLS        4
#define SCREEN_LINES       192

/* When the top-left pixel of the shown border is drawn: Fuse's
   machine->line_times[0], 3777 */
#define LINE_TIMES_0 \
  ( TOP_LEFT_PIXEL - BORDER_LINES * TS2068_TSTATES_PER_LINE - 4 * BORDER_COLS )

/* An instruction can run past the end of the frame before the frame
   event is taken: room for the longest, contended */
#define CONTENTION_SLACK   256

/* The z80_glue.h state */
libspectrum_dword tstates;
libspectrum_dword z80_interrupt_length = INTERRUPT_LENGTH;
int z80_int_disabled;

/* z80_machine.h: whether each 8K chunk is contended now, and the delay
   at each T-state of the frame (Fuse's ula_contention) */
libspectrum_byte ts2068_contended[ 8 ];
libspectrum_byte ts2068_contention[ TS2068_TSTATES_PER_FRAME + CONTENTION_SLACK ];

/* Memory. HOME: the 16K ROM, then RAM at 0x4000-0xFFFF (Fuse's RAM pages
   5, 2 and 0); RAM5, the screen, is contended. EXROM: 8K, or 16K with
   the even chunks seeing its first half and the odd ones its second.
   DOCK: FFh without a cartridge. */
static libspectrum_byte home_rom[ 0x4000 ];
static libspectrum_byte ram[ 0xc000 ];
static libspectrum_byte exrom[ 0x4000 ];
static size_t exrom_size;
static libspectrum_byte empty_chunk[ 0x2000 ];

/* A DOCK cartridge (Fuse's dck.c): the 8K chunks a .dck file gives the
   DOCK, EXROM and HOME banks. ROM chunks are read-only. RAM chunks start
   from the file's contents (or zeros) at each insert and hard reset;
   HOME RAM chunks are copied into RAM then, as Fuse does. Everything
   lives in one allocation, so on the P4 it lands in PSRAM. */
enum { DCK_DOCK, DCK_EXROM, DCK_HOME, DCK_BANKS };
#define DCK_NULL      0         /* the file's chunk types */
#define DCK_RAM_EMPTY 1
#define DCK_ROM       2
#define DCK_RAM       3

static struct {
  libspectrum_byte type;        /* DCK_NULL: the bank's own chunk */
  const libspectrum_byte *file; /* its contents, or NULL: zeros */
  libspectrum_byte *page;       /* what's mapped; NULL for HOME RAM */
} dck[ DCK_BANKS ][ 8 ];
static libspectrum_byte *dck_memory;

static const libspectrum_byte *read_map[ 8 ];
static libspectrum_byte *write_map[ 8 ];   /* NULL: read-only */

static int fuse_dock_contention;

/* Ports */
static libspectrum_byte scld_dec;          /* port FF */
static libspectrum_byte scld_hsr;          /* port F4 */
static libspectrum_byte ula_last_byte;     /* port FE */
static libspectrum_byte ula_default_value;
static libspectrum_byte keyboard[ 8 ];
static libspectrum_byte joystick[ 2 ];

static struct {
  libspectrum_byte current_register;
  libspectrum_byte registers[ 16 ];
} ay;

static const libspectrum_byte ay_mask[ 16 ] = {
  0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0x1f, 0xff,
  0x1f, 0x1f, 0x1f, 0xff, 0xff, 0x0f, 0xff, 0xff,
};

static ts2068_tspico_fn tspico;
static void *tspico_ctx;

/* Frames */
static int frame_event;
static libspectrum_dword frame_count;
static int flash_frame_count, flash_reversed;

#define SCLD_ALTDFILE   0x01
#define SCLD_EXTCOLOUR  0x02
#define SCLD_HIRES      0x04
#define SCLD_INTDISABLE 0x40
#define SCLD_ALTMEMBANK 0x80

#define ALTDFILE_OFFSET 0x2000

/* The picture as the beam drew it: each 8-pixel chunk of the shown
   picture, 40 across and 240 down, copied once the beam has passed it */
#define BEAM_COLS   ( TS2068_SCREEN_WIDTH / 16 )
#define BEAM_CHUNKS ( BEAM_COLS * TS2068_SCREEN_HEIGHT )
#define CHUNK_HIRES 0x80        /* in paper */

typedef struct {
  libspectrum_byte data, data2, ink, paper;
} beam_chunk;

static beam_chunk beam[ BEAM_CHUNKS ];
static int beam_done;           /* chunks copied this frame */

static void beam_update( void );

/* ---- memory ------------------------------------------------------------ */

/* Fuse's tc2068_memory_map: HOME everywhere, then the chunks whose HSR
   bit is set from EXROM (DEC bit 7 set) or DOCK. Fuse marks DOCK and
   EXROM at 0x4000-0x7FFF contended (scld_set_exrom_dock_contention).
   Not Fuse: on the TS2068 only HOME RAM is contended, unless
   fuse_dock_contention asks for Fuse's behaviour. */
static void
memory_map( void )
{
  int c;

  for( c = 0; c < 8; c++ ) {
    int screen_chunk = ( c == 2 || c == 3 );

    if( scld_hsr & ( 1 << c ) ) {
      int bank = scld_dec & SCLD_ALTMEMBANK ? DCK_EXROM : DCK_DOCK;

      if( dck[ bank ][ c ].page ) {
        read_map[ c ] = dck[ bank ][ c ].page;
        write_map[ c ] = dck[ bank ][ c ].type == DCK_ROM ?
                         NULL : dck[ bank ][ c ].page;
      } else {
        if( bank == DCK_EXROM )
          read_map[ c ] = exrom +
            ( exrom_size == 0x4000 && ( c & 1 ) ? 0x2000 : 0 );
        else
          read_map[ c ] = empty_chunk;
        write_map[ c ] = NULL;
      }
      ts2068_contended[ c ] = fuse_dock_contention && screen_chunk;
    } else if( dck[ DCK_HOME ][ c ].page ) {
      /* A cartridge's ROM over HOME; its RAM over the ROM chunks */
      read_map[ c ] = dck[ DCK_HOME ][ c ].page;
      write_map[ c ] = dck[ DCK_HOME ][ c ].type == DCK_ROM ?
                       NULL : dck[ DCK_HOME ][ c ].page;
      ts2068_contended[ c ] = screen_chunk;
    } else if( c < 2 ) {
      read_map[ c ] = home_rom + c * 0x2000;
      write_map[ c ] = NULL;
      ts2068_contended[ c ] = 0;
    } else {
      write_map[ c ] = ram + ( c - 2 ) * 0x2000;
      read_map[ c ] = write_map[ c ];
      ts2068_contended[ c ] = screen_chunk;
    }
  }
}

libspectrum_byte
readbyte_internal( libspectrum_word address )
{
  return read_map[ address >> 13 ][ address & 0x1fff ];
}

libspectrum_byte
readbyte( libspectrum_word address )
{
  if( ts2068_contended[ address >> 13 ] )
    tstates += ts2068_contention[ tstates ];
  tstates += 3;

  return read_map[ address >> 13 ][ address & 0x1fff ];
}

void
writebyte( libspectrum_word address, libspectrum_byte b )
{
  libspectrum_byte *chunk = write_map[ address >> 13 ];

  if( ts2068_contended[ address >> 13 ] )
    tstates += ts2068_contention[ tstates ];
  tstates += 3;

  if( chunk ) {
    /* The display files, HOME RAM 0x4000-0x5AFF and 0x6000-0x7AFF:
       what the beam has passed is drawn first (Fuse's
       memory_display_dirty_sinclair) */
    if( ( address & 0xc000 ) == 0x4000 && ( address & 0x1fff ) < 0x1b00 &&
        chunk[ address & 0x1fff ] != b )
      beam_update();
    chunk[ address & 0x1fff ] = b;
  }
}

/* ---- contention -------------------------------------------------------- */

/* Fuse's spectrum_contend_delay_65432100 (contend_delay_common with
   offset 1), which Fuse uses for the TS2068. Whether the SCLD contends
   this way is unmeasured (PLAN.md §1.6, Q10a). */
static libspectrum_byte
contend_delay( libspectrum_dword time )
{
  static const libspectrum_byte pattern[ 8 ] = { 5, 4, 3, 2, 1, 0, 0, 6 };
  int line, tstates_through_line;

  line = (libspectrum_signed_dword)( time - LINE_TIMES_0 ) /
         TS2068_TSTATES_PER_LINE;

  tstates_through_line = time - LINE_TIMES_0 +
                         ( LEFT_BORDER - BORDER_COLS * 4 );
  tstates_through_line %= TS2068_TSTATES_PER_LINE;

  if( line < BORDER_LINES || line >= BORDER_LINES + SCREEN_LINES ) return 0;
  if( tstates_through_line < LEFT_BORDER - 1 ) return 0;
  if( tstates_through_line >= LEFT_BORDER + HORIZONTAL_SCREEN - 1 ) return 0;

  return pattern[ tstates_through_line % 8 ];
}

/* Fuse's ula_contend_port_early/late, with tc2048_port_from_ula: the
   SCLD answers F4, FE and FF */
static int
port_from_scld( libspectrum_word port )
{
  port &= 0xff;
  return port == 0xf4 || port == 0xfe || port == 0xff;
}

static void
contend_port_early( libspectrum_word port )
{
  if( ts2068_contended[ port >> 13 ] )
    tstates += ts2068_contention[ tstates ];
  tstates++;
}

static void
contend_port_late( libspectrum_word port )
{
  if( port_from_scld( port ) ) {
    tstates += ts2068_contention[ tstates ]; tstates += 2;
  } else if( ts2068_contended[ port >> 13 ] ) {
    tstates += ts2068_contention[ tstates ]; tstates++;
    tstates += ts2068_contention[ tstates ]; tstates++;
    tstates += ts2068_contention[ tstates ];
  } else {
    tstates += 2;
  }
}

/* ---- ports ------------------------------------------------------------- */

static void
scld_dec_write( libspectrum_byte b )
{
  libspectrum_byte old = scld_dec;

  scld_dec = b;
  z80_int_disabled = !!( b & SCLD_INTDISABLE );

  /* Reenabled interrupts: a retriggered interrupt */
  if( ( old & SCLD_INTDISABLE ) && !( b & SCLD_INTDISABLE ) )
    z80_interrupt();

  if( ( old ^ b ) & SCLD_ALTMEMBANK ) memory_map();
}

static libspectrum_byte
keyboard_read( libspectrum_byte porth )
{
  libspectrum_byte data = 0xff;
  int i;

  for( i = 0; i < 8; i++, porth >>= 1 )
    if( !( porth & 0x01 ) ) data &= keyboard[ i ];

  return data;
}

static libspectrum_byte
ay_registerport_read( void )
{
  int current = ay.current_register;

  if( current == 14 )
    return ay.registers[ 7 ] & 0x40 ? 0xbf & ay.registers[ 14 ] : 0xbf;
  if( current == 15 && !( ay.registers[ 7 ] & 0x80 ) ) return 0xff;

  return ay.registers[ current ] & ay_mask[ current ];
}

static libspectrum_byte
tspico_access( libspectrum_word port, int out, libspectrum_byte b )
{
  int status = ( port & 0xff ) == 0x0f;
  int op = out ? ( status ? TS2068_TSPICO_OUT_STATUS : TS2068_TSPICO_OUT_DATA )
               : ( status ? TS2068_TSPICO_IN_STATUS : TS2068_TSPICO_IN_DATA );

  return tspico ? tspico( op, b, tspico_ctx ) : 0xff;
}

/* Every TS2068 port is decoded on the low byte, and no two share one,
   so Fuse's peripheral list is a switch. An unattached port reads FFh
   (spectrum_unattached_port_none: the 2068 has no floating bus). */
static libspectrum_byte
port_read( libspectrum_word port )
{
  libspectrum_byte r;

  switch( port & 0xff ) {

  case 0xfe:
    return ula_default_value & keyboard_read( port >> 8 );

  case 0xf4:
    return scld_hsr;

  case 0xff:
    return scld_dec;

  case 0xf5:
    if( ay.current_register == 14 ) return 0xff;
    return ay_registerport_read();

  case 0xf6:
    if( ay.current_register != 14 ) return ay_registerport_read();
    r = ay.registers[ 7 ] & 0x40 ? ay.registers[ 14 ] : 0xff;
    if( port & 0x0100 ) r &= ~joystick[ 0 ];
    if( port & 0x0200 ) r &= ~joystick[ 1 ];
    return r;

  case 0x0e: case 0x0f:
    return tspico_access( port, 0, 0 );

  default:
    return 0xff;

  }
}

static void
port_write( libspectrum_word port, libspectrum_byte b )
{
  switch( port & 0xff ) {

  case 0xfe:
    beam_update();
    ula_last_byte = b;
    /* Fuse's ula_write: MIC (bit 3) is active low, EAR (bit 4) high */
    sound_ula( tstates, !( b & 0x08 ), !!( b & 0x10 ) );
    ula_default_value = 0x5f;   /* Fuse: ts2068_current->timex */
    break;

  case 0xf4:
    scld_hsr = b;
    memory_map();
    break;

  case 0xff:
    beam_update();
    scld_dec_write( b );
    break;

  case 0xf5:
    ay.current_register = b & 0x0f;
    break;

  case 0xf6:
    ay.registers[ ay.current_register ] = b & ay_mask[ ay.current_register ];
    if( sound_enabled ) sound_ay_write( ay.current_register, b, tstates );
    break;

  case 0x0e: case 0x0f:
    tspico_access( port, 1, b );
    break;

  }
}

libspectrum_byte
readport( libspectrum_word port )
{
  libspectrum_byte b;

  contend_port_early( port );
  contend_port_late( port );
  b = port_read( port );
  tstates++;

  return b;
}

void
writeport( libspectrum_word port, libspectrum_byte b )
{
  contend_port_early( port );
  port_write( port, b );
  contend_port_late( port ); tstates++;
}

/* ---- frames ------------------------------------------------------------ */

/* Fuse's spectrum_frame_event_fn and spectrum_frame */
static void
frame_event_fn( libspectrum_dword event_tstates, int type, void *user_data )
{
  (void)event_tstates; (void)type; (void)user_data;

  /* Fuse's display_frame: the rest of the picture, then a new one */
  beam_update();
  beam_done = 0;

  event_frame( TS2068_TSTATES_PER_FRAME );
  tstates -= TS2068_TSTATES_PER_FRAME;
  z80_frame( TS2068_TSTATES_PER_FRAME );
  sound_frame( TS2068_TSTATES_PER_FRAME );

  /* display_render_frame: FLASH swaps every 16 frames */
  if( ++flash_frame_count == 16 ) {
    flash_reversed = 1;
  } else if( flash_frame_count == 32 ) {
    flash_reversed = 0;
    flash_frame_count = 0;
  }

  event_add( TS2068_TSTATES_PER_FRAME, frame_event );
  frame_count++;

  z80_interrupt();
}

void
ts2068_run_frame( void )
{
  libspectrum_dword frame = frame_count;

  while( frame_count == frame ) {
    z80_do_opcodes();
    event_do_events();
  }
}

libspectrum_dword
ts2068_frame_count( void )
{
  return frame_count;
}

/* ---- setup ------------------------------------------------------------- */

/* ---- DOCK cartridges -------------------------------------------------- */

/* The cartridge's RAM chunks from the file (or zeros), and its HOME RAM
   chunks into RAM */
static void
dck_load_ram( void )
{
  int b, c;

  for( b = 0; b < DCK_BANKS; b++ )
    for( c = 0; c < 8; c++ ) {
      libspectrum_byte type = dck[ b ][ c ].type;
      libspectrum_byte *to;

      if( type != DCK_RAM && type != DCK_RAM_EMPTY ) continue;
      to = dck[ b ][ c ].page ? dck[ b ][ c ].page : ram + ( c - 2 ) * 0x2000;
      if( dck[ b ][ c ].file ) memcpy( to, dck[ b ][ c ].file, 0x2000 );
      else memset( to, 0, 0x2000 );
    }
}

static int
dck_bank( libspectrum_byte id )
{
  return id == 0 ? DCK_DOCK : id == 254 ? DCK_EXROM : id == 255 ? DCK_HOME : -1;
}

int
ts2068_dock_insert( const uint8_t *file, size_t size )
{
  size_t at, need = 0;
  libspectrum_byte *mem;
  int c;

  /* Check it all, and count the memory: a copy of each chunk the file
     holds, and a live copy of each RAM chunk outside HOME RAM */
  for( at = 0; at < size; ) {
    int b, chunks = 0;

    if( size - at < 9 || ( b = dck_bank( file[ at ] ) ) < 0 )
      return TS2068_DCK_CORRUPT;
    for( c = 0; c < 8; c++ ) {
      libspectrum_byte type = file[ at + 1 + c ];

      if( type > DCK_RAM ) return TS2068_DCK_CORRUPT;
      if( type == DCK_ROM || type == DCK_RAM ) chunks++;
      if( ( type == DCK_RAM || type == DCK_RAM_EMPTY ) &&
          !( b == DCK_HOME && c >= 2 ) ) need += 0x2000;
    }
    if( size - at - 9 < (size_t)chunks * 0x2000 ) return TS2068_DCK_CORRUPT;
    need += chunks * 0x2000;
    at += 9 + chunks * 0x2000;
  }
  if( !need ) return TS2068_DCK_CORRUPT;
  mem = malloc( need );
  if( !mem ) return TS2068_DCK_MEMORY;

  ts2068_dock_eject();
  dck_memory = mem;
  for( at = 0; at < size; ) {
    int b = dck_bank( file[ at ] );
    const libspectrum_byte *types = file + at + 1;

    at += 9;
    for( c = 0; c < 8; c++ ) {
      libspectrum_byte type = types[ c ];
      const libspectrum_byte *data = NULL;

      if( type == DCK_NULL ) continue;
      if( type == DCK_ROM || type == DCK_RAM ) {
        memcpy( mem, file + at, 0x2000 );
        data = mem;
        mem += 0x2000;
        at += 0x2000;
      }
      dck[ b ][ c ].type = type;
      dck[ b ][ c ].file = data;
      if( type == DCK_ROM )
        dck[ b ][ c ].page = (libspectrum_byte *)data;
      else if( b == DCK_HOME && c >= 2 )
        dck[ b ][ c ].page = NULL;
      else {
        dck[ b ][ c ].page = mem;
        mem += 0x2000;
      }
    }
  }
  dck_load_ram();
  memory_map();
  return 0;
}

void
ts2068_dock_eject( void )
{
  free( dck_memory );
  dck_memory = NULL;
  memset( dck, 0, sizeof( dck ) );
  memory_map();
}

int
ts2068_dock_present( void )
{
  return dck_memory != NULL;
}

int
ts2068_init( const ts2068_config_t *config )
{
  static int core_ready;
  libspectrum_dword t;

  if( !config->home_rom || !config->exrom ||
      ( config->exrom_size != 0x2000 && config->exrom_size != 0x4000 ) )
    return -1;

  if( !core_ready ) {
    z80_init( NULL );
    frame_event = event_register( frame_event_fn, "End of frame" );
    core_ready = 1;
  }

  memcpy( home_rom, config->home_rom, sizeof( home_rom ) );
  memcpy( exrom, config->exrom, config->exrom_size );
  exrom_size = config->exrom_size;
  memset( empty_chunk, 0xff, sizeof( empty_chunk ) );
  tspico = config->tspico;
  tspico_ctx = config->tspico_ctx;
  fuse_dock_contention = config->fuse_dock_contention;

  for( t = 0; t < TS2068_TSTATES_PER_FRAME; t++ )
    ts2068_contention[ t ] = contend_delay( t );

  memset( keyboard, 0xff, sizeof( keyboard ) );
  memset( joystick, 0, sizeof( joystick ) );

  ts2068_reset( 1 );
  return 0;
}

/* Fuse's ts2068_select and ts2068_reset, with the TS2068's and the
   modules' resets in the order Fuse runs them. Not Fuse: a hard reset
   clears RAM (power-on); Fuse's RAM is only zero when it starts. */
void
ts2068_reset( int hard_reset )
{
  if( hard_reset ) {
    memset( ram, 0, sizeof( ram ) );
    dck_load_ram();             /* Fuse's dck_reset reloads the file */
  }

  sound_ay_reset();             /* Fuse's machine_reset */
  event_reset();
  tstates = 0;
  event_add( TS2068_TSTATES_PER_FRAME, frame_event );

  scld_hsr = 0;
  scld_dec = 0;
  z80_int_disabled = 0;
  ula_last_byte = 0;
  ula_default_value = 0xff;
  ay.current_register = 0;
  memset( ay.registers, 0, sizeof( ay.registers ) );
  memory_map();

  z80_reset( hard_reset );

  frame_count = 0;
  beam_done = 0;
}

/* ---- for the platform -------------------------------------------------- */

int
ts2068_sound_init( int sample_rate )
{
  if( sound_init( sample_rate, TS2068_CPU_HZ, TS2068_TSTATES_PER_FRAME ) )
    return -1;
  sound_ay_reset();
  return 0;
}

const int16_t *
ts2068_sound_samples( int *count )
{
  return sound_samples( count );
}

libspectrum_byte
ts2068_peek( libspectrum_word address )
{
  return readbyte_internal( address );
}

void
ts2068_poke( libspectrum_word address, libspectrum_byte value )
{
  libspectrum_byte *chunk = write_map[ address >> 13 ];

  if( chunk ) chunk[ address & 0x1fff ] = value;
}

libspectrum_byte *
ts2068_home_ram( void )
{
  return ram;
}

void
ts2068_set_keyboard( const libspectrum_byte rows[ 8 ] )
{
  memcpy( keyboard, rows, sizeof( keyboard ) );
}

void
ts2068_set_joystick( int which, libspectrum_byte bits )
{
  joystick[ which & 1 ] = bits;
}

/* ---- the screen -------------------------------------------------------- */

/* Fuse follows the beam in 8-pixel chunks (docs/NOTES.md, Q7): before a
   write to the display files, port FE or port FF lands, every chunk the
   beam has passed is copied, as display_write_if_dirty_timex,
   display_get_attr_byte and the border would draw it from the RAM and
   ports as they are then (display_dirty.c's copy_critical_region,
   display_border.c's border changes); the rest at the end of the frame. A
   chunk is copied once the beam is past its last pixel: the one at line
   y, column x starts at LINE_TIMES_0 + 224 y + 4 x. */

static void
parse_attr( libspectrum_byte attr, libspectrum_byte *ink,
            libspectrum_byte *paper )
{
  if( ( attr & 0x80 ) && flash_reversed ) {
    *ink = ( attr & ( 0x0f << 3 ) ) >> 3;
    *paper = ( attr & 0x07 ) + ( ( attr & 0x40 ) >> 3 );
  } else {
    *ink = ( attr & 0x07 ) + ( ( attr & 0x40 ) >> 3 );
    *paper = ( attr & ( 0x0f << 3 ) ) >> 3;
  }
}

/* scld.c's hires_convert_dec: the attribute that HIRES's colour pair,
   DEC bits 3-5, stands for */
static libspectrum_byte
hires_attr( void )
{
  static const libspectrum_byte attrs[ 8 ] = {
    0x78, 0x71, 0x6a, 0x63, 0x5c, 0x55, 0x4e, 0x47
  };
  return attrs[ ( scld_dec >> 3 ) & 0x07 ];
}

static int
line_start( int y )
{
  return ( ( y & 0xc0 ) << 5 ) | ( ( y & 0x07 ) << 8 ) | ( ( y & 0x38 ) << 2 );
}

static int
attr_start( int y )
{
  return 0x1800 + 32 * ( y / 8 );
}

/* Copy chunks beam_done to end - 1, all on one line */
static void
beam_copy_line( int end )
{
  const libspectrum_byte *screen = ram;         /* RAM5 */
  int line = beam_done / BEAM_COLS, col = beam_done % BEAM_COLS;
  int y = line - BORDER_LINES;
  int hires = !!( scld_dec & SCLD_HIRES );
  int alt = scld_dec & SCLD_ALTDFILE ? ALTDFILE_OFFSET : 0;
  libspectrum_byte border, ink = 0, paper = 0;

  if( hires ) {
    parse_attr( hires_attr(), &ink, &paper );
    border = paper;
  } else {
    border = ula_last_byte & 0x07;
  }

  for( ; beam_done < end; beam_done++, col++ ) {
    beam_chunk *c = &beam[ beam_done ];
    int x = col - BORDER_COLS, offset;

    if( y < 0 || y >= SCREEN_LINES || x < 0 || x >= 32 ) {
      c->data = c->data2 = 0;
      c->ink = c->paper = border;
      continue;
    }

    offset = line_start( y ) + x + alt;
    c->data = screen[ offset ];
    if( hires ) {
      switch( scld_dec & 0x07 ) {
      case 0x05: offset = attr_start( y ) + x + ALTDFILE_OFFSET; break;
      case 0x06: offset = line_start( y ) + x + ALTDFILE_OFFSET; break;
      case 0x07: break;
      default:   offset = attr_start( y ) + x; break;
      }
      c->data2 = screen[ offset ];
      c->ink = ink;
      c->paper = paper | CHUNK_HIRES;
    } else {
      libspectrum_byte attr;

      if( scld_dec & SCLD_EXTCOLOUR )
        attr = screen[ line_start( y ) + x + ALTDFILE_OFFSET ];
      else
        attr = screen[ attr_start( y ) + x + alt ];
      parse_attr( attr, &c->ink, &c->paper );
      c->data2 = 0;
    }
  }
}

/* Copy the chunks the beam has passed (Fuse's display_get_beam_position:
   it stops at the end of the shown picture) */
static void
beam_update( void )
{
  libspectrum_signed_dword t =
    (libspectrum_signed_dword)tstates - LINE_TIMES_0;
  int line, col, end;

  if( t < 0 ) return;
  line = t / TS2068_TSTATES_PER_LINE;
  if( line >= TS2068_SCREEN_HEIGHT ) {
    end = BEAM_CHUNKS;
  } else {
    col = ( t % TS2068_TSTATES_PER_LINE ) / 4;
    end = line * BEAM_COLS + ( col < BEAM_COLS ? col : BEAM_COLS );
  }

  while( beam_done < end ) {
    int line_end = ( beam_done / BEAM_COLS + 1 ) * BEAM_COLS;
    beam_copy_line( end < line_end ? end : line_end );
  }
}

void
ts2068_render_line( int line, libspectrum_byte *row )
{
  int col, i;

  for( col = 0; col < BEAM_COLS; col++, row += 16 ) {
    const beam_chunk *c = &beam[ line * BEAM_COLS + col ];
    libspectrum_byte ink = c->ink, paper = c->paper & 0x0f;

    if( c->paper & CHUNK_HIRES ) {
      for( i = 0; i < 8; i++ ) {
        row[ i ] = c->data & ( 0x80 >> i ) ? ink : paper;
        row[ 8 + i ] = c->data2 & ( 0x80 >> i ) ? ink : paper;
      }
    } else {
      for( i = 0; i < 8; i++ )
        row[ 2 * i ] = row[ 2 * i + 1 ] = c->data & ( 0x80 >> i ) ? ink : paper;
    }
  }
}

void
ts2068_render( libspectrum_byte *pixels )
{
  int line;

  for( line = 0; line < TS2068_SCREEN_HEIGHT; line++ )
    ts2068_render_line( line, pixels + line * TS2068_SCREEN_WIDTH );
}

/* ---- state ------------------------------------------------------------- */

void
ts2068_get_state( ts2068_state_t *out )
{
  z80_regs_t r;

  z80_get_regs( &r );
  out->af = r.af; out->bc = r.bc; out->de = r.de; out->hl = r.hl;
  out->af_ = r.af_; out->bc_ = r.bc_; out->de_ = r.de_; out->hl_ = r.hl_;
  out->ix = r.ix; out->iy = r.iy; out->sp = r.sp; out->pc = r.pc;
  out->i = r.i; out->r = r.r;
  out->iff1 = r.iff1; out->iff2 = r.iff2; out->im = r.im;
  out->halted = r.halted;
  out->frame_tstate = tstates;
  out->dec = scld_dec; out->hsr = scld_hsr; out->border = ula_last_byte & 0x07;
}

/* ---- snapshots (machine/szx.c) ----------------------------------------- */

void
ts2068_snap_get( ts2068_snap_t *s )
{
  z80_regs_t r;

  z80_get_regs( &r );
  s->af = r.af; s->bc = r.bc; s->de = r.de; s->hl = r.hl;
  s->af_ = r.af_; s->bc_ = r.bc_; s->de_ = r.de_; s->hl_ = r.hl_;
  s->ix = r.ix; s->iy = r.iy; s->sp = r.sp; s->pc = r.pc; s->memptr = r.memptr;
  s->i = r.i; s->r = r.r; s->iff1 = r.iff1; s->iff2 = r.iff2; s->im = r.im;
  s->halted = r.halted; s->ei_last = r.ei_last; s->f_set = r.f_set;
  s->tstates = tstates;
  s->ula = ula_last_byte; s->hsr = scld_hsr; s->dec = scld_dec;
  s->ay_register = ay.current_register;
  memcpy( s->ay, ay.registers, sizeof( s->ay ) );
}

/* Fuse's snapshot_copy_from: machine_reset_from_snapshot, then the
   modules' snapshot_from hooks (ula, z80, scld, ay), then memory_map */
void
ts2068_snap_set( const ts2068_snap_t *s )
{
  z80_regs_t r;
  int i;

  ts2068_reset( 0 );

  port_write( 0xfe, s->ula );   /* ula_from_snapshot, at T-state 0 */
  tstates = s->tstates;

  r.af = s->af; r.bc = s->bc; r.de = s->de; r.hl = s->hl;
  r.af_ = s->af_; r.bc_ = s->bc_; r.de_ = s->de_; r.hl_ = s->hl_;
  r.ix = s->ix; r.iy = s->iy; r.sp = s->sp; r.pc = s->pc; r.memptr = s->memptr;
  r.i = s->i; r.r = s->r; r.iff1 = s->iff1; r.iff2 = s->iff2; r.im = s->im;
  r.halted = s->halted; r.ei_last = s->ei_last; r.f_set = s->f_set;
  z80_set_regs( &r );

  port_write( 0xf4, s->hsr );
  scld_dec_write( s->dec );

  ay.current_register = s->ay_register & 0x0f;
  for( i = 0; i < 16; i++ ) {
    ay.registers[ i ] = s->ay[ i ];
    if( sound_enabled ) sound_ay_write( i, s->ay[ i ], 0 );
  }

  memory_map();
}
