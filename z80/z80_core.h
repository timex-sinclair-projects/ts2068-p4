/* z80_core.h: what the machine and the tests call a Z80 core through.

   Two cores implement it: Fuse's (z80/, with z80_core_fuse.c) and
   Z80_JLS (z80_jls/). A build links exactly one. Both reach the machine
   only through z80_glue.h, so the machine, coretest and zexrun run on
   either unchanged. (PLAN.md Phase 2: "Wrap two cores behind the same Z80
   interface".)

   Conventions are Fuse's: while halted, PC is the address of the HALT;
   R is the full 8 bits; tstates counts from the frame interrupt. */

#ifndef Z80_CORE_H
#define Z80_CORE_H

#include "libspectrum.h"

typedef struct {
  libspectrum_word af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc, memptr;
  libspectrum_byte i, r, iff1, iff2, im, halted;
  /* As an SZX snapshot has them: the last instruction was EI (no
     interrupt before the next), and it set F (Fuse's Q, which SCF and CCF
     read). Set them after tstates, which ei_last is relative to. */
  libspectrum_byte ei_last, f_set;
} z80_regs_t;

/* Set up the core's tables and register its events. Call once. */
int z80_init( void *context );
void z80_reset( int hard_reset );

/* Run instructions until tstates reaches event_next_event (a prefixed
   instruction runs to its end). */
void z80_do_opcodes( void );

/* The /INT line may be low: take the interrupt now if the core would
   (IFF1 set, not straight after EI, tstates < z80_interrupt_length and
   z80_int_disabled clear). Nonzero if it was taken. */
int z80_interrupt( void );

/* The machine moved tstates back by frame_length at the end of a frame:
   adjust any times the core keeps. */
void z80_frame( libspectrum_dword frame_length );

void z80_get_regs( z80_regs_t *regs );
void z80_set_regs( const z80_regs_t *regs );

/* Which core this is: "fuse" or "jls" */
extern const char z80_core_name[];

#endif /* Z80_CORE_H */
