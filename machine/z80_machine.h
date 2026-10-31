/* z80_machine.h: the TS2068's contention, as z80/z80_glue.h asks for.

   Fuse's macros (z80/z80_macros.h), with its 2K memory_page flags replaced
   by one flag per 8K chunk: the SCLD pages in 8K chunks, and whether a
   chunk is contended depends on what is mapped there (ts2068.c). The
   TS2068 has one delay table for MREQ and no-MREQ cycles alike. */

#ifndef Z80_MACHINE_H
#define Z80_MACHINE_H

#include "libspectrum.h"

extern libspectrum_byte machine_contended[ 8 ];
extern libspectrum_byte machine_contention[];

#define contend_read(address,time) \
  if( machine_contended[ (libspectrum_word)(address) >> 13 ] ) \
    tstates += machine_contention[ tstates ]; \
  tstates += (time);

#define contend_read_no_mreq(address,time) \
  if( machine_contended[ (libspectrum_word)(address) >> 13 ] ) \
    tstates += machine_contention[ tstates ]; \
  tstates += (time);

#define contend_write_no_mreq(address,time) \
  if( machine_contended[ (libspectrum_word)(address) >> 13 ] ) \
    tstates += machine_contention[ tstates ]; \
  tstates += (time);

#endif /* Z80_MACHINE_H */
