/* z80_core_fuse.c: z80_core.h for Fuse's core (z80.c, z80_ops.c).
   z80_init, z80_reset, z80_do_opcodes and z80_interrupt are Fuse's own;
   this adds the rest. */

#include "z80_glue.h"
#include "z80_core.h"
#include "z80.h"
#include "z80_macros.h"

const char z80_core_name[] = "fuse";

void
z80_frame( libspectrum_dword frame_length )
{
  if( z80.interrupts_enabled_at >= 0 )
    z80.interrupts_enabled_at -= frame_length;
}

void
z80_get_regs( z80_regs_t *r )
{
  r->af = AF; r->bc = BC; r->de = DE; r->hl = HL;
  r->af_ = AF_; r->bc_ = BC_; r->de_ = DE_; r->hl_ = HL_;
  r->ix = IX; r->iy = IY; r->sp = SP; r->pc = PC; r->memptr = z80.memptr.w;
  r->i = I; r->r = ( R7 & Z80_R7_MASK ) | ( R & Z80_R_LOWER_BITS );
  r->iff1 = IFF1; r->iff2 = IFF2; r->im = IM; r->halted = z80.halted;
  r->ei_last = z80.interrupts_enabled_at == (libspectrum_signed_dword)tstates;
  r->f_set = !!z80.q;
}

void
z80_set_regs( const z80_regs_t *r )
{
  AF = r->af; BC = r->bc; DE = r->de; HL = r->hl;
  AF_ = r->af_; BC_ = r->bc_; DE_ = r->de_; HL_ = r->hl_;
  IX = r->ix; IY = r->iy; SP = r->sp; PC = r->pc; z80.memptr.w = r->memptr;
  I = r->i; R = R7 = r->r;
  IFF1 = r->iff1; IFF2 = r->iff2; IM = r->im; z80.halted = r->halted;
  /* Fuse's z80_from_snapshot */
  z80.interrupts_enabled_at = r->ei_last ? (libspectrum_signed_dword)tstates : -1;
  z80.q = r->f_set ? F : 0;
}
