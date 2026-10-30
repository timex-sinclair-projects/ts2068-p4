/* z80_glue.h: everything the Z80 core needs from the machine around it.

   In Fuse the core reaches into the rest of the emulator (event.h,
   memory_pages.h, settings.h, rzx.h, the debugger and a dozen disk and
   interface peripherals). The lifted core reaches only what is declared
   here, and the TS2068 machine (or test/z80/coretest.c) provides it.
   z80/README.md lists every change from Fuse's files. */

#ifndef Z80_GLUE_H
#define Z80_GLUE_H

#include "libspectrum.h"

/* The clock: T-states since the start of the frame, and when the next
   scheduled event is due. z80_do_opcodes() runs until tstates reaches
   event_next_event. */
extern libspectrum_dword tstates;
extern libspectrum_dword event_next_event;

/* Events, as Fuse's event.h: register a handler once, then schedule it. */
typedef void (*event_fn_t)( libspectrum_dword event_time, int type,
                            void *user_data );
int event_register( event_fn_t fn, const char *description );
void event_add( libspectrum_dword event_time, int type );

/* Memory and I/O. readbyte/writebyte apply contention and add the 3
   T-states of the access; readbyte_internal is the bare read used for the
   opcode fetch (after contend_read( PC, 4 )). readport/writeport do the
   whole I/O cycle, contention included. */
libspectrum_byte readbyte( libspectrum_word address );
libspectrum_byte readbyte_internal( libspectrum_word address );
void writebyte( libspectrum_word address, libspectrum_byte b );
libspectrum_byte readport( libspectrum_word port );
void writeport( libspectrum_word port, libspectrum_byte b );

/* Contention for the cycles that aren't a plain read or write. With
   CORETEST these are functions (the core tester logs them); otherwise the
   machine provides them as macros in z80_machine.h, which must keep
   Fuse's behaviour: `if the page is contended, add the delay for this
   T-state; then add time`. */
#ifdef CORETEST
void contend_read( libspectrum_word address, libspectrum_dword time );
void contend_read_no_mreq( libspectrum_word address, libspectrum_dword time );
void contend_write_no_mreq( libspectrum_word address, libspectrum_dword time );
#else
#include "z80_machine.h"
#endif

/* The /INT line. An interrupt is accepted only while
   tstates < z80_interrupt_length (32 on the TS2068) and the SCLD's
   INTDISABLE bit (port FF bit 6) is clear: z80_int_disabled mirrors it. */
extern libspectrum_dword z80_interrupt_length;
extern int z80_int_disabled;

/* The TS2068's Z80 is NMOS: OUT (C),0 writes 00h, and SCF/CCF take bits 3
   and 5 from (Q ^ F) | A. */
#define Z80_IS_CMOS 0

/* Fuse's tape and SLT traps, compiled into the generated opcode code:
   never trapped here (the TS-Pico is the tape). */
#define tape_save_trap() 1
#define tape_load_trap() 1
#define slt_trap( address, level ) ( (void)0 )

/* RZX playback bookkeeping that the core still updates; nothing reads it. */
extern int rzx_instructions_offset;

#endif /* Z80_GLUE_H */
