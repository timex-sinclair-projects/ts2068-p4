/* p4_audio.c: the CrowPanel Advanced 7"'s speakers (p4_audio.h).

   The pins, the I2S format (standard Philips, 16-bit, stereo) and the
   amplifier enable come from Elecrow's Lesson12 example
   (peripheral/bsp_audio/bsp_audio.c), not its code (third_party/
   PROVENANCE.md, "Facts taken from other sources"). */

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include "p4_audio.h"

#define AUDIO_GPIO_LRCLK 21
#define AUDIO_GPIO_BCLK  22
#define AUDIO_GPIO_DATA  23
#define AUDIO_GPIO_AMP   30             /* low: amplifiers on */

#define DMA_DESCS 4
#define DMA_FRAMES ( P4_AUDIO_BUFFER_FRAMES / DMA_DESCS )
#define CHUNK 256                       /* stereo frames converted at a time */

static i2s_chan_handle_t tx;
static int rate;
static int enabled;
static int volume = 25;                 /* of 256: 10% */
static volatile uint32_t underruns;
static uint32_t underruns_before;       /* silence before the first write */
static uint64_t samples_written;
static uint32_t wait_us_max;
static int16_t chunk[ CHUNK * 2 ];

/* The send queue overflowed: the DMA had nothing new, so auto_clear sent
   silence */
static bool IRAM_ATTR
on_send_q_ovf( i2s_chan_handle_t handle, i2s_event_data_t *event, void *ctx )
{
  (void)handle; (void)event; (void)ctx;
  underruns++;
  return false;
}

esp_err_t
p4_audio_init( int sample_rate )
{
  esp_err_t err;

  if( tx ) return sample_rate == rate ? ESP_OK : ESP_ERR_INVALID_STATE;

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG( I2S_NUM_AUTO,
                                                           I2S_ROLE_MASTER );
  chan_cfg.dma_desc_num = DMA_DESCS;
  chan_cfg.dma_frame_num = DMA_FRAMES;
  chan_cfg.auto_clear = true;
  err = i2s_new_channel( &chan_cfg, &tx, NULL );
  if( err != ESP_OK ) { tx = NULL; return err; }

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG( sample_rate ),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG( I2S_DATA_BIT_WIDTH_16BIT,
                                                     I2S_SLOT_MODE_STEREO ),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = AUDIO_GPIO_BCLK,
      .ws = AUDIO_GPIO_LRCLK,
      .dout = AUDIO_GPIO_DATA,
      .din = I2S_GPIO_UNUSED,
    },
  };
  i2s_event_callbacks_t cbs = { .on_send_q_ovf = on_send_q_ovf };
  if( ( err = i2s_channel_init_std_mode( tx, &std_cfg ) ) != ESP_OK ||
      ( err = i2s_channel_register_event_callback( tx, &cbs, NULL ) ) != ESP_OK ) {
    i2s_del_channel( tx );
    tx = NULL;
    return err;
  }
  rate = sample_rate;

  const gpio_config_t amp = {
    .pin_bit_mask = 1ULL << AUDIO_GPIO_AMP,
    .mode = GPIO_MODE_OUTPUT,
  };
  gpio_set_level( AUDIO_GPIO_AMP, 1 );
  gpio_config( &amp );
  return ESP_OK;
}

void
p4_audio_start( void )
{
  if( !tx || enabled ) return;
  samples_written = 0;
  wait_us_max = 0;
  i2s_channel_enable( tx );
  gpio_set_level( AUDIO_GPIO_AMP, 0 );
  enabled = 1;
}

void
p4_audio_stop( void )
{
  if( !enabled ) return;
  gpio_set_level( AUDIO_GPIO_AMP, 1 );
  i2s_channel_disable( tx );
  enabled = 0;
}

void
p4_audio_volume( int percent )
{
  if( percent < 0 ) percent = 0;
  if( percent > 100 ) percent = 100;
  volume = percent * 256 / 100;
}

uint32_t
p4_audio_write( const int16_t *samples, int count )
{
  int64_t t0 = esp_timer_get_time();

  if( !enabled ) return 0;
  if( !samples_written ) underruns_before = underruns;
  samples_written += count;
  while( count > 0 ) {
    int n = count < CHUNK ? count : CHUNK, i;
    size_t done;
    for( i = 0; i < n; i++ ) {
      int16_t s = ( samples[ i ] * volume ) >> 8;
      chunk[ 2 * i ] = chunk[ 2 * i + 1 ] = s;
    }
    i2s_channel_write( tx, chunk, n * 2 * sizeof( int16_t ), &done,
                       portMAX_DELAY );
    samples += n;
    count -= n;
  }

  uint32_t waited = esp_timer_get_time() - t0;
  if( waited > wait_us_max ) wait_us_max = waited;
  return waited;
}

void
p4_audio_get_stats( p4_audio_stats_t *stats )
{
  stats->samples = samples_written;
  stats->underruns = underruns - underruns_before;
  stats->wait_us_max = wait_us_max;
}
