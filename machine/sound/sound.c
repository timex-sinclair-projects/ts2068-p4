/* sound.c: the TS2068's sound: the ULA and the AY, through Fuse's TV
   speaker model, in mono.

   Lifted from fuse-tspico's sound.c and sound/output_mixer.c:
   Copyright (c) 2000-2026 Russell Marks, Matan Ziv-Av, Philip Kendall,
                           Fredrick Meunier, Patrik Rak

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

   What Fuse does for a TS2068, and nothing else: the speaker type
   resolves to TV, so the ULA's level (MIC and EAR, port FE) goes through
   the ULA filter, the AY goes into the TV buffer, and their sum goes
   through the TV filter. No stereo AY, no tape, no Specdrum, Covox or
   Currah. Fuse's main buffer, which nothing writes on a TS2068, is gone:
   it only set how many samples a frame reads, which is the same for every
   buffer. The filters and the mix work in sound_real (sound_real.h):
   Fuse's double on the host, float on the P4. Volumes are Fuse's defaults (100). The Blip_Buffers hold 100 ms
   rather than Fuse's 1 s; the length changes no sample. */

#include <stdlib.h>

#include "sound.h"
#include "ay_engine.h"
#include "blipbuffer.h"
#include "tv_filter.h"
#include "ula_filter.h"

/* Fuse's sound.h */
#define SOUND_AMPL_BEEPER ( 50 * 256 )
#define SOUND_AMPL_TAPE   ( 2 * 256 )
#define SOUND_STEREO_AY_NONE 0

#define BUFFER_MSEC 100

int sound_enabled = 0;

static int sound_framesiz;
static Blip_Buffer *tv_buf, *ula_buf;
static Blip_Synth *ula_synth;
static blip_sample_t *samples, *tv_samples, *ula_samples;
static int sample_count;
static tv_filter_t tv_filter;
static ula_filter_t ula_filter;
static int ula_mic_on, ula_beeper_on;
/* Fuse's sound_tv_route == -1: the AY's outputs and the TV filter are set
   again at the next frame */
static int routes_pending;

static int
output_buffer_init( Blip_Buffer **buffer, libspectrum_dword clock_rate,
                    int sample_rate )
{
  *buffer = new_Blip_Buffer();
  if( !*buffer ) return 1;
  blip_buffer_set_clock_rate( *buffer, clock_rate );
  if( blip_buffer_set_sample_rate( *buffer, sample_rate, BUFFER_MSEC ) )
    return 1;
  blip_buffer_set_bass_freq( *buffer, 0 );
  return 0;
}

/* Fuse's sound_init and output_mixer_init. Once only. */
int
sound_init( int sample_rate, libspectrum_dword clock_rate,
            libspectrum_dword tstates_per_frame )
{
  float hz;

  if( sound_enabled ) return 0;

  hz = ( float )clock_rate / tstates_per_frame;
  sound_framesiz = ( float )sample_rate / hz + 1;

  if( output_buffer_init( &ula_buf, clock_rate, sample_rate ) ||
      output_buffer_init( &tv_buf, clock_rate, sample_rate ) )
    return 1;
  ula_synth = new_Blip_Synth();
  if( !ula_synth ) return 1;
  blip_synth_set_volume( ula_synth, 1.0 );
  blip_synth_set_output( ula_synth, ula_buf );
  blip_synth_set_treble_eq( ula_synth, 0.0 );

  if( tv_filter_configure( &tv_filter, sample_rate ) ||
      ula_filter_configure( &ula_filter, sample_rate ) )
    return 1;

  samples = calloc( sound_framesiz, sizeof( blip_sample_t ) );
  tv_samples = calloc( sound_framesiz, sizeof( blip_sample_t ) );
  ula_samples = calloc( sound_framesiz, sizeof( blip_sample_t ) );
  if( !samples || !tv_samples || !ula_samples ) return 1;

  ay_engine_init( 100, SOUND_STEREO_AY_NONE );
  ay_engine_set_outputs( tv_buf, NULL, SOUND_STEREO_AY_NONE );

  sound_enabled = 1;
  routes_pending = 1;
  return 0;
}

void
sound_ay_write( int reg, int val, libspectrum_dword now )
{
  ay_engine_write( reg, val, now );
}

static int
ula_level( void )
{
  /* sound_ula_levels' MIC node: the TV speaker's input */
  return ( ula_mic_on ? SOUND_AMPL_TAPE : 0 ) +
         ( ula_beeper_on ? SOUND_AMPL_BEEPER : 0 );
}

/* Fuse's sound_ay_reset, with output_mixer_reset */
void
sound_ay_reset( void )
{
  ay_engine_reset();

  tv_filter_reset( &tv_filter );
  ula_filter_reset( &ula_filter );
  if( sound_enabled ) {
    blip_buffer_clear( ula_buf, BLIP_BUFFER_DEF_ENTIRE_BUFF );
    blip_synth_set_output( ula_synth, ula_buf );
    blip_synth_set_level( ula_synth, ula_level() );
  }
  sample_count = 0;
  routes_pending = 1;
}

void
sound_ula( libspectrum_dword at_tstates, int mic_on, int beeper_on )
{
  ula_mic_on = !!mic_on;
  ula_beeper_on = !!beeper_on;
  if( !sound_enabled ) return;
  blip_synth_update( ula_synth, at_tstates, ula_level() );
}

static long
clip_sample( sound_real sample )
{
  if( sample > 0x7fff ) return 0x7fff;
  if( sample < -0x8000 ) return -0x8000;
  return sample;
}

/* Fuse's sound_frame, with output_mixer_end_frame and mix_output */
void
sound_frame( libspectrum_dword tstates_per_frame )
{
  long count, tv_count, ula_count, i;

  if( !sound_enabled ) return;

  if( routes_pending ) {
    ay_engine_set_outputs( tv_buf, NULL, SOUND_STEREO_AY_NONE );
    tv_filter_reset( &tv_filter );
    routes_pending = 0;
  }

  ay_engine_render( tstates_per_frame );

  blip_buffer_end_frame( tv_buf, tstates_per_frame );
  blip_buffer_end_frame( ula_buf, tstates_per_frame );
  count = blip_buffer_samples_avail( tv_buf );
  if( count > sound_framesiz ) count = sound_framesiz;
  tv_count = blip_buffer_read_samples( tv_buf, tv_samples, count, 0 );
  ula_count = blip_buffer_read_samples( ula_buf, ula_samples, count, 0 );

  for( i = 0; i < count; i++ ) {
    sound_real sample = 0;
    if( i < ula_count ) {
      sample = ula_filter_apply( &ula_filter, ula_samples[i] );
      if( i < tv_count ) sample += tv_samples[i];
      sample = tv_filter_apply( &tv_filter, sample );
    }
    samples[i] = clip_sample( sample );
  }
  sample_count = count;

  ay_engine_end_frame();
}

const libspectrum_signed_word *
sound_samples( int *count )
{
  *count = sample_count;
  return samples;
}
