/* machine.h: the TS2068 behind one small API (PLAN.md §1.2).

   The platform layer (the P4, or a host tool) sees only this header: it
   loads ROMs, runs one frame at a time, reads the screen and memory, and
   presses keys. The Z80 core, the SCLD, the memory map and the contention
   model stay behind it, so the core or the machine can be swapped without
   touching the platform. */

#ifndef TS2068_H
#define TS2068_H

#include <stdint.h>
#include <stddef.h>

#define TS2068_CPU_HZ            3528000
#define TS2068_AY_HZ             ( TS2068_CPU_HZ / 2 )
#define TS2068_TSTATES_PER_LINE  224
#define TS2068_LINES_PER_FRAME   262
#define TS2068_TSTATES_PER_FRAME ( TS2068_TSTATES_PER_LINE * TS2068_LINES_PER_FRAME )

/* Bridge v1 frame ops (tspico/p4bridge/p4bridge.h, docs/EMULATOR_BRIDGE.md). */
enum {
  TS2068_TSPICO_OUT_DATA = 0,
  TS2068_TSPICO_IN_DATA = 1,
  TS2068_TSPICO_IN_STATUS = 2,
  TS2068_TSPICO_OUT_STATUS = 3,
};

/* A Z80 access to ports 0Eh/0Fh: returns the byte read (ignored for an
   OUT). NULL: no TS-Pico plugged in, and the ports read FFh. */
typedef uint8_t ( *ts2068_tspico_fn )( uint8_t op, uint8_t value, void *ctx );

typedef struct {
  const uint8_t *home_rom;      /* 16K */
  const uint8_t *exrom;         /* 8K, or 16K as on the TS-Pico */
  size_t exrom_size;
  ts2068_tspico_fn tspico;
  void *tspico_ctx;
  /* 1: DOCK and EXROM at 0x4000-0x7FFF are contended, as in Fuse, for
     comparing against fuse-tspico. 0 (the TS2068): only HOME RAM is
     contended (PLAN.md §3, "Contention"). */
  int fuse_dock_contention;
} ts2068_config_t;

/* Load the ROMs and hard-reset. 0 on success, -1 for a bad config. */
int ts2068_init( const ts2068_config_t *config );
void ts2068_reset( int hard_reset );

/* Run until the next frame interrupt: one 60.1145 Hz frame. */
void ts2068_run_frame( void );
uint32_t ts2068_frame_count( void );

/* Memory as the Z80 sees it now (paging applied), taking no time. */
uint8_t ts2068_peek( uint16_t address );
void ts2068_poke( uint16_t address, uint8_t value );
/* HOME RAM: 0x4000-0xFFFF, 48K. */
uint8_t *ts2068_home_ram( void );

/* Keyboard: the eight half-rows, port FE's A8..A15; a 0 bit is a key down.
   joystick: the bits the TS2068 sticks pull low on AY R14 (sticks 0 and 1). */
void ts2068_set_keyboard( const uint8_t rows[8] );
void ts2068_set_joystick( int which, uint8_t bits );

/* The screen at the end of the last frame, 640 x 240, one byte per pixel:
   the colour 0-15 (8-15 bright). The 512-wide picture with a 64-pixel
   border left and right, 24 lines top and bottom; low-resolution modes
   are doubled across. Drawn as the beam passed, in 8-pixel chunks, as
   Fuse draws it: a change to the display files, the border or port FF's
   modes shows from where the beam was when it was made. */
#define TS2068_SCREEN_WIDTH  640
#define TS2068_SCREEN_HEIGHT 240
void ts2068_render( uint8_t *pixels );

/* One line of the same picture, 0-239, into TS2068_SCREEN_WIDTH bytes:
   so a display can take the picture a line at a time without holding
   all 150K of it. */
void ts2068_render_line( int line, uint8_t *row );

/* Sound: Fuse's TS2068 sound, the ULA (port FE's MIC and EAR bits) and
   the AY, through Fuse's TV speaker model, in mono. Off until
   ts2068_sound_init(), which makes each frame produce samples at
   sample_rate (Hz); call it after ts2068_init() and before the first
   frame, as Fuse starts its sound before the machine runs. Once only.
   0 on success, -1 if out of memory or the rate is too low (the TV
   filter needs more than 20 kHz). */
int ts2068_sound_init( int sample_rate );

/* The samples the last frame made, 16-bit signed: about sample_rate /
   60.1145 of them (733 or 734 at 44.1 kHz), *count says how many. Valid
   until the next frame. */
const int16_t *ts2068_sound_samples( int *count );

/* The CPU and SCLD state, for tests and the comparison against Fuse. */
typedef struct {
  uint16_t af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
  uint8_t i, r, iff1, iff2, im, halted;
  uint32_t frame_tstate;
  uint8_t dec, hsr, border;
} ts2068_state_t;

void ts2068_get_state( ts2068_state_t *out );

/* Everything an SZX snapshot holds besides HOME RAM (ts2068_home_ram):
   the Z80, with the EI and F flags SZX keeps; the T-state in the frame;
   the last bytes written to ports FE, F4 and FF; the AY's register file
   and selected register. */
typedef struct {
  uint16_t af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc, memptr;
  uint8_t i, r, iff1, iff2, im, halted, ei_last, f_set;
  uint32_t tstates;
  uint8_t ula, hsr, dec;
  uint8_t ay_register, ay[ 16 ];
} ts2068_snap_t;

void ts2068_snap_get( ts2068_snap_t *snap );

/* Restore it as Fuse loads a snapshot: a soft reset (RAM kept: load HOME
   RAM before or after), then port FE, the T-state, the Z80, ports F4 and
   FF, and the AY, whose registers are taken as they are, unmasked, and
   given to the sound. Frame counts and the FLASH phase are not part of
   it, and an interrupt still to come from an EI just before is lost, as
   in Fuse. */
void ts2068_snap_set( const ts2068_snap_t *snap );

/* SZX (zx-state) snapshots, as libspectrum writes them for a TS2068
   (machine/szx.c). Save writes the machine as it is now, between frames,
   into buf: uncompressed, TS2068_SZX_SIZE bytes; 0 if size is too
   small. Load restores one, compressed or not; 0, or a TS2068_SZX_ error.
   A snapshot with DOCK pages (a cartridge) can't be loaded: this machine
   has no DOCK. After an error in the RAM pages, HOME RAM may be partly
   loaded and the machine should be reset. */
#define TS2068_SZX_SIZE ( 8 + 8 + 37 + 8 + 8 + 6 + 8 + 5 + 3 * ( 8 + 3 + 0x4000 ) + \
                          8 + 18 + 8 + 2 + 8 + 2 )
size_t ts2068_szx_save( uint8_t *buf, size_t size );
int ts2068_szx_load( const uint8_t *buf, size_t len );

#define TS2068_SZX_NOT_SZX      -1      /* no ZXST header, or not a TS2068 */
#define TS2068_SZX_CORRUPT      -2      /* a chunk's length is wrong */
#define TS2068_SZX_UNSUPPORTED  -3      /* DOCK pages, or a version before 1.1 */
#define TS2068_SZX_INFLATE      -4      /* a compressed page didn't inflate */

#endif /* TS2068_H */
