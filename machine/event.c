/* event.c: the event queue z80/z80_glue.h asks for.

   Fuse's event.c semantics without glib or allocation: events are kept
   sorted by time, then by type (the order they were registered); a new
   event goes before any equal one, as g_slist_insert_sorted puts it.
   Only a handful are ever pending (the frame, the Z80's interrupt and
   IFF2 events), so a small array is enough. */

#include <stdlib.h>

#include "event.h"

#define MAX_TYPES  8
#define MAX_EVENTS 16

libspectrum_dword event_next_event = EVENT_NONE;

static struct {
  event_fn_t fn;
} types[ MAX_TYPES ];
static int n_types;

static struct {
  libspectrum_dword tstates;
  int type;
} queue[ MAX_EVENTS ];
static int n_queued;

int
event_register( event_fn_t fn, const char *description )
{
  (void)description;
  if( n_types == MAX_TYPES ) abort();
  types[ n_types ].fn = fn;
  return n_types++;
}

static int
before( libspectrum_dword t1, int type1, libspectrum_dword t2, int type2 )
{
  return t1 != t2 ? t1 < t2 : type1 <= type2;
}

void
event_add( libspectrum_dword event_time, int type )
{
  int i, j;

  if( n_queued == MAX_EVENTS ) abort();

  for( i = 0; i < n_queued; i++ )
    if( before( event_time, type, queue[ i ].tstates, queue[ i ].type ) )
      break;
  for( j = n_queued; j > i; j-- ) queue[ j ] = queue[ j - 1 ];
  queue[ i ].tstates = event_time;
  queue[ i ].type = type;
  n_queued++;
  event_next_event = queue[ 0 ].tstates;
}

void
event_do_events( void )
{
  while( event_next_event <= tstates ) {
    libspectrum_dword t = queue[ 0 ].tstates;
    int type = queue[ 0 ].type, i;

    /* Remove the event before running it: it may add events */
    for( i = 1; i < n_queued; i++ ) queue[ i - 1 ] = queue[ i ];
    n_queued--;
    event_next_event = n_queued ? queue[ 0 ].tstates : EVENT_NONE;

    if( types[ type ].fn ) types[ type ].fn( t, type, NULL );
  }
}

void
event_frame( libspectrum_dword tstates_per_frame )
{
  int i;

  for( i = 0; i < n_queued; i++ ) queue[ i ].tstates -= tstates_per_frame;
  event_next_event = n_queued ? queue[ 0 ].tstates : EVENT_NONE;
}

void
event_reset( void )
{
  n_queued = 0;
  event_next_event = EVENT_NONE;
}
