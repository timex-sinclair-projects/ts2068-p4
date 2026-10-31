/* machine.h: the TS2068 behind one small API (PLAN.md §1.2).

   The platform layer (the P4, or a host tool) sees only this header: it
   loads ROMs, runs one frame at a time, reads the screen and memory, and
   presses keys. The Z80 core, the SCLD, the memory map and the contention
   model stay behind it, so the core or the machine can be swapped without
   touching the platform. */

#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>
#include <stddef.h>

#define MACHINE_CPU_HZ            3528000
#define MACHINE_AY_HZ             ( MACHINE_CPU_HZ / 2 )
#define MACHINE_TSTATES_PER_LINE  224
#define MACHINE_LINES_PER_FRAME   262
#define MACHINE_TSTATES_PER_FRAME ( MACHINE_TSTATES_PER_LINE * MACHINE_LINES_PER_FRAME )

/* Bridge v1 frame ops (tspico/p4bridge/p4bridge.h, docs/EMULATOR_BRIDGE.md). */
enum {
  MACHINE_TSPICO_OUT_DATA = 0,
  MACHINE_TSPICO_IN_DATA = 1,
  MACHINE_TSPICO_IN_STATUS = 2,
  MACHINE_TSPICO_OUT_STATUS = 3,
};

/* A Z80 access to ports 0Eh/0Fh: returns the byte read (ignored for an
   OUT). NULL: no TS-Pico plugged in, and the ports read FFh. */
typedef uint8_t ( *machine_tspico_fn )( uint8_t op, uint8_t value, void *ctx );

typedef struct {
  const uint8_t *home_rom;      /* 16K */
  const uint8_t *exrom;         /* 8K, or 16K as on the TS-Pico */
  size_t exrom_size;
  machine_tspico_fn tspico;
  void *tspico_ctx;
  /* 1: DOCK and EXROM at 0x4000-0x7FFF are contended, as in Fuse, for
     comparing against fuse-tspico. 0 (the TS2068): only HOME RAM is
     contended (PLAN.md §3, "Contention"). */
  int fuse_dock_contention;
} machine_config_t;

/* Load the ROMs and hard-reset. 0 on success, -1 for a bad config. */
int machine_init( const machine_config_t *config );
void machine_reset( int hard_reset );

/* Run until the next frame interrupt: one 60.1145 Hz frame. */
void machine_run_frame( void );
uint32_t machine_frame_count( void );

/* Memory as the Z80 sees it now (paging applied), taking no time. */
uint8_t machine_peek( uint16_t address );
void machine_poke( uint16_t address, uint8_t value );
/* HOME RAM: 0x4000-0xFFFF, 48K. */
uint8_t *machine_home_ram( void );

/* Keyboard: the eight half-rows, port FE's A8..A15; a 0 bit is a key down.
   joystick: the bits the TS2068 sticks pull low on AY R14 (sticks 0 and 1). */
void machine_set_keyboard( const uint8_t rows[8] );
void machine_set_joystick( int which, uint8_t bits );

/* The screen at the end of the last frame, 640 x 240, one byte per pixel:
   the colour 0-15 (8-15 bright). The 512-wide picture with a 64-pixel
   border left and right, 24 lines top and bottom; low-resolution modes
   are doubled across. Drawn from the screen RAM once a frame, not with
   the beam: mid-frame changes are not shown yet. */
#define MACHINE_SCREEN_WIDTH  640
#define MACHINE_SCREEN_HEIGHT 240
void machine_render( uint8_t *pixels );

/* The CPU and SCLD state, for tests and the comparison against Fuse. */
typedef struct {
  uint16_t af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
  uint8_t i, r, iff1, iff2, im, halted;
  uint32_t frame_tstate;
  uint8_t dec, hsr, border;
} machine_state_t;

void machine_get_state( machine_state_t *out );

#endif /* MACHINE_H */
