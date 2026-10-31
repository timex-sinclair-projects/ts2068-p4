/* p4_audio.h: the CrowPanel Advanced 7"'s speakers, for the TS2068's
   sound.

   Two NS4168 I2S amplifiers on one I2S bus (LRCLK GPIO21, BCLK GPIO22,
   DATA GPIO23), one on each channel, enabled together by GPIO30 (active
   low). The TS2068's sound is mono, so each sample goes to both. The I2S
   clock runs the emulator: writing a frame's samples waits for room in
   the DMA buffers, so a loop that runs a frame and then writes its
   samples runs at the TS2068's speed (PLAN.md Phase 3, "Audio"). */

#ifndef P4_AUDIO_H
#define P4_AUDIO_H

#include <stdint.h>

#include "esp_err.h"

/* About 23 ms at 44.1 kHz: the most that can be queued ahead */
#define P4_AUDIO_BUFFER_FRAMES 1024

/* Set up the I2S output at sample_rate (Hz), stopped. Once; a later call
   with the same rate does nothing. */
esp_err_t p4_audio_init( int sample_rate );

/* Start the I2S, playing silence until the first write, enable the
   amplifiers, and start the stats again. Stop disables both, so nothing
   plays (or counts as an underrun) between runs. */
void p4_audio_start( void );
void p4_audio_stop( void );

/* 0 (mute) to 100. The default is 10: 50 was far too loud on the panel's
   speakers. */
void p4_audio_volume( int percent );

/* Queue count mono samples. Waits, if the buffers are full, until they
   all fit; returns the time waited in us. */
uint32_t p4_audio_write( const int16_t *samples, int count );

typedef struct {
  uint64_t samples;             /* samples written since start */
  uint32_t underruns;           /* times the DMA ran dry and played silence,
                                   since the first write after start */
  uint32_t wait_us_max;         /* longest p4_audio_write wait */
} p4_audio_stats_t;

void p4_audio_get_stats( p4_audio_stats_t *stats );

#endif /* P4_AUDIO_H */
