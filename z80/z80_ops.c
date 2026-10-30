/* z80_ops.c: Process the next opcode
   Copyright (c) 1999-2018 Philip Kendall, Witold Filipczyk
   Copyright (c) 2015-2023 Stuart Brady
   Copyright (c) 2015-2017 Gergely Szasz
   Copyright (c) 2015 Sergio Baldoví

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

#include "z80_glue.h"
#include "z80.h"

#include "z80_macros.h"

#ifndef HAVE_ENOUGH_MEMORY
static int z80_cbxx( libspectrum_byte opcode2 );
static int z80_ddxx( libspectrum_byte opcode2 );
static int z80_edxx( libspectrum_byte opcode2 );
static int z80_fdxx( libspectrum_byte opcode2 );
static void z80_ddfdcbxx( libspectrum_byte opcode3 );
#endif				/* #ifndef HAVE_ENOUGH_MEMORY */

/* Fuse checks a list of per-instruction hooks here (profiler, RZX,
   debugger, and the paging traps of a dozen Spectrum interfaces), skipped
   with gcc computed gotos when they're off. The TS2068 needs only the
   NMOS IFF2 one, so it's a plain test (z80/README.md). */

#ifndef HAVE_ENOUGH_MEMORY
static libspectrum_byte opcode = 0x00;
#endif

/* Execute Z80 opcodes until the next event */
void
z80_do_opcodes( void )
{
#ifdef HAVE_ENOUGH_MEMORY
  libspectrum_byte opcode = 0x00;
#endif
  libspectrum_byte last_Q;

  while( tstates < event_next_event ) {

    contend_read( PC, 4 );

    /* Do the instruction fetch; readbyte_internal used here to avoid
       triggering read breakpoints */
    opcode = readbyte_internal( PC );

    if( z80.iff2_read ) {
      z80.iff2_read = 0;
      /* Execute *one* instruction before reevaluating the checks */
      event_add( tstates, z80_nmos_iff2_event );
    }

  end_opcode:
    PC++; R++;
    last_Q = Q; /* keep Q value from previous opcode for SCF and CCF */
    Q = 0;      /* preempt Q value assuming next opcode doesn't set flags */

    switch(opcode) {
#include "z80/opcodes_base.c"
    }

  }

}

#ifndef HAVE_ENOUGH_MEMORY

static int
z80_cbxx( libspectrum_byte opcode2 )
{
  switch(opcode2) {
#include "z80/z80_cb.c"
  }
  return 0;
}

static int
z80_ddxx( libspectrum_byte opcode2 )
{
  switch(opcode2) {
#define REGISTER  IX
#define REGISTERL IXL
#define REGISTERH IXH
#include "z80/z80_ddfd.c"
#undef REGISTERH
#undef REGISTERL
#undef REGISTER
  }
  return 0;
}

static int
z80_edxx( libspectrum_byte opcode2 )
{
  switch(opcode2) {
#include "z80/z80_ed.c"
  }
  return 0;
}

static int
z80_fdxx( libspectrum_byte opcode2 )
{
  switch(opcode2) {
#define REGISTER  IY
#define REGISTERL IYL
#define REGISTERH IYH
#include "z80/z80_ddfd.c"
#undef REGISTERH
#undef REGISTERL
#undef REGISTER
  }
  return 0;
}

static void
z80_ddfdcbxx( libspectrum_byte opcode3 )
{
  switch(opcode3) {
#include "z80/z80_ddfdcb.c"
  }
}

#endif			/* #ifndef HAVE_ENOUGH_MEMORY */
