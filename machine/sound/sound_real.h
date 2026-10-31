/* sound_real.h: the type of the sound filters' arithmetic.

   esp32-2068, not Fuse. Fuse's filters (tv_filter.c, ula_filter.c) and
   its mixer work in double. The P4's FPU is single precision only, so
   there double is done in software, and it cost 4 ms a frame. The P4
   build defines SOUND_REAL as float; the host keeps double, so
   test/machine's comparison with Fuse stays sample for sample. */

#ifndef SOUND_REAL_H
#define SOUND_REAL_H

#ifndef SOUND_REAL
#define SOUND_REAL double
#endif

typedef SOUND_REAL sound_real;

#endif /* SOUND_REAL_H */
