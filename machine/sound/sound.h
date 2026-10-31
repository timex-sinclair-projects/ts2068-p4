/* sound.h: the TS2068's sound, inside the machine.

   Fuse's sound.c and sound/output_mixer.c, cut to what Fuse does for a
   TS2068: the TS2068 has no beeper of its own (libspectrum gives it no
   BEEPER capability), so Fuse plays the ULA's output and the AY through
   its "TV" speaker model, in mono. machine/ts2068.c calls these at the
   points Fuse does; the platform sees only ts2068_sound_init() and
   ts2068_sound_samples() in machine.h. */

#ifndef TS2068_SOUND_H
#define TS2068_SOUND_H

#include "libspectrum.h"

extern int sound_enabled;

int sound_init( int sample_rate, libspectrum_dword clock_rate,
                libspectrum_dword tstates_per_frame );
void sound_ay_write( int reg, int val, libspectrum_dword now );
void sound_ay_reset( void );
void sound_ula( libspectrum_dword at_tstates, int mic_on, int beeper_on );
void sound_frame( libspectrum_dword tstates_per_frame );

/* The samples sound_frame() made, until the next sound_frame() */
const libspectrum_signed_word *sound_samples( int *count );

#endif /* TS2068_SOUND_H */
