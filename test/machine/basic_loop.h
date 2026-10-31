/* basic_loop.h: type the benchmark's BASIC loop (basic_loop.c). */

#ifndef BASIC_LOOP_H
#define BASIC_LOOP_H

/* Frames to let the ROM boot before typing */
#define BASIC_LOOP_BOOT_FRAMES 150
/* By this frame RUN has been entered and the loop is running */
#define BASIC_LOOP_RUNNING_FRAMES ( BASIC_LOOP_BOOT_FRAMES + 17 * 8 + 10 )

/* Call before running frame number `frame` (from 0 after reset): sets the
   keyboard for it. */
void basic_loop_frame( unsigned frame );

#endif /* BASIC_LOOP_H */
