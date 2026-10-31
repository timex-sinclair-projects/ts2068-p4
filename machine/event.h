/* event.h: the machine's side of the event queue (event.c). */

#ifndef MACHINE_EVENT_H
#define MACHINE_EVENT_H

#include "z80_glue.h"

#define EVENT_NONE 0xffffffffu

/* Run every event due by now (tstates). */
void event_do_events( void );
/* The frame wrapped: move every pending event back by its length. */
void event_frame( libspectrum_dword tstates_per_frame );
/* Drop every pending event (types stay registered). */
void event_reset( void );

#endif /* MACHINE_EVENT_H */
