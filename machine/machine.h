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
   are doubled across. Drawn from the screen RAM once a frame, not with
   the beam: mid-frame changes are not shown yet. */
#define TS2068_SCREEN_WIDTH  640
#define TS2068_SCREEN_HEIGHT 240
void ts2068_render( uint8_t *pixels );

/* One line of the same picture, 0-239, into TS2068_SCREEN_WIDTH bytes:
   so a display can take the picture a line at a time without holding
   all 150K of it. */
void ts2068_render_line( int line, uint8_t *row );

/* The CPU and SCLD state, for tests and the comparison against Fuse. */
typedef struct {
  uint16_t af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
  uint8_t i, r, iff1, iff2, im, halted;
  uint32_t frame_tstate;
  uint8_t dec, hsr, border;
} ts2068_state_t;

void ts2068_get_state( ts2068_state_t *out );

#endif /* TS2068_H */
