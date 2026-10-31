/* p4_display.c: the CrowPanel Advanced 7"'s MIPI-DSI panel.

   Board facts from Elecrow's V1.2 examples (Lesson07, bsp_illuminate.c
   and main.c) and schematic: DSI PHY powered from the P4's LDO3 at
   2.5 V; 2 data lanes at 900 Mbps; the DPI timing below; backlight PWM
   on GPIO31 at 30 kHz; the panel reset left to a software reset, as
   Elecrow does. The EK79007's start-up commands are those of Espressif's
   esp_lcd_ek79007 1.0.2 (Apache-2.0), which Elecrow uses; the driver is
   small enough that this file sends them itself rather than pulling in a
   managed component (third_party/PROVENANCE.md). */

#include <stdbool.h>
#include <string.h>

#include "driver/ledc.h"
#include "driver/ppa.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "p4_display.h"

static const char *TAG = "p4_display";

#define DSI_PHY_LDO_CHANNEL 3
#define DSI_PHY_LDO_MV      2500
#define DSI_LANES           2
#define DSI_LANE_MBPS       900

#define BACKLIGHT_GPIO      31
#define BACKLIGHT_HZ        30000
/* The highest LEDC timer and channel, out of the way of machine.PWM,
   which allocates from 0 */
#define BACKLIGHT_TIMER     LEDC_TIMER_3
#define BACKLIGHT_CHANNEL   LEDC_CHANNEL_7
#define BACKLIGHT_BITS      LEDC_TIMER_11_BIT

#define TV_WIDTH            640         /* TS2068_SCREEN_WIDTH */
#define TV_HEIGHT           240         /* TS2068_SCREEN_HEIGHT, doubled */

/* EK79007 registers (esp_lcd_ek79007.c) */
#define EK79007_PAD_CONTROL 0xb2
#define EK79007_DSI_2_LANE  0x10

static const struct {
  uint8_t cmd, data, delay_ms;
} ek79007_init[] = {
  { 0x80, 0x8b, 0 },
  { 0x81, 0x78, 0 },
  { 0x82, 0x84, 0 },
  { 0x83, 0x88, 0 },
  { 0x84, 0xa8, 0 },
  { 0x85, 0xe3, 0 },
  { 0x86, 0x88, 0 },
};

/* Fuse's TS2068 colours (rgb_colours in ui/win32/win32display.c and the
   other UIs), as RGB565 */
#define RGB565( r, g, b ) \
  ( ( ( (r) & 0xf8 ) << 8 ) | ( ( (g) & 0xfc ) << 3 ) | ( (b) >> 3 ) )
static const uint16_t palette[ 16 ] = {
  RGB565(   0,   0,   0 ), RGB565(   0,   0, 192 ),
  RGB565( 192,   0,   0 ), RGB565( 192,   0, 192 ),
  RGB565(   0, 192,   0 ), RGB565(   0, 192, 192 ),
  RGB565( 192, 192,   0 ), RGB565( 192, 192, 192 ),
  RGB565(   0,   0,   0 ), RGB565(   0,   0, 255 ),
  RGB565( 255,   0,   0 ), RGB565( 255,   0, 255 ),
  RGB565(   0, 255,   0 ), RGB565(   0, 255, 255 ),
  RGB565( 255, 255,   0 ), RGB565( 255, 255, 255 ),
};

static esp_ldo_channel_handle_t phy_ldo;
static esp_lcd_dsi_bus_handle_t dsi_bus;
static esp_lcd_panel_io_handle_t dbi_io;
static esp_lcd_panel_handle_t panel;
/* Three frame buffers: one being scanned, one flipped and waiting for
   the next refresh, and one to draw into, so drawing never waits */
#define NUM_FBS P4_DISPLAY_BUFFERS
static uint16_t *fb[ NUM_FBS ];
static volatile int flipped;            /* scanned from the next refresh */
static volatile int scanning;           /* being scanned now */
static volatile uint32_t refreshes;
static uint32_t fb_lines[ NUM_FBS ][ TV_HEIGHT ];  /* hash of each line held */
static uint32_t palette2[ 256 ];        /* two pixels, ( b << 4 ) | a */

/* Changed lines are converted to RGB565 in internal RAM, a strip of up to
   STRIP_LINES consecutive lines at a time, and the PPA copies each strip
   into the frame buffer twice, once to the even rows and once to the odd
   (doubling the lines without scaling: the frame buffer is treated as 300
   rows of 2048 pixels, each holding two panel rows). Two strips, so the
   CPU converts one while the PPA copies the other. The CPU never writes
   the frame buffers in PSRAM, which it can only do at about 56 MB/s. */
#define STRIP_LINES 16
static uint16_t *strip[ 2 ];
static SemaphoreHandle_t strip_free[ 2 ];
static ppa_client_handle_t ppa;
static uint8_t line_buf[ TV_WIDTH ] __attribute__(( aligned( 4 ) ));
static p4_display_stats_t stats;
static bool redraw_all;
static p4_display_side_fn side;

/* A refresh finished. On this chip (before revision 3) that's the DMA's
   end of transfer, at which it restarts on the frame buffer the last flip
   chose (esp_lcd_panel_dpi.c, mipi_dsi_dma_trans_done_cb) */
static IRAM_ATTR bool
refresh_done( esp_lcd_panel_handle_t p, esp_lcd_dpi_panel_event_data_t *e,
              void *ctx )
{
  (void)p; (void)e; (void)ctx;
  scanning = flipped;
  refreshes++;
  return false;
}

/* The PPA finished a copy. The second copy of a strip carries the strip's
   semaphore; the PPA does its transactions in order, so the first is done
   too. */
static IRAM_ATTR bool
strip_copied( ppa_client_handle_t client, ppa_event_data_t *e, void *ctx )
{
  (void)client; (void)e;
  BaseType_t woken = pdFALSE;
  if( ctx ) xSemaphoreGiveFromISR( (SemaphoreHandle_t)ctx, &woken );
  return woken == pdTRUE;
}

static esp_err_t
backlight_init( void )
{
  const ledc_timer_config_t timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = BACKLIGHT_BITS,
    .timer_num = BACKLIGHT_TIMER,
    .freq_hz = BACKLIGHT_HZ,
    .clk_cfg = LEDC_USE_PLL_DIV_CLK,
  };
  const ledc_channel_config_t channel = {
    .gpio_num = BACKLIGHT_GPIO,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = BACKLIGHT_CHANNEL,
    .timer_sel = BACKLIGHT_TIMER,
    .duty = 0,
  };
  ESP_RETURN_ON_ERROR( ledc_timer_config( &timer ), TAG, "backlight timer" );
  return ledc_channel_config( &channel );
}

void
p4_display_backlight( int percent )
{
  if( percent < 0 ) percent = 0;
  if( percent > 100 ) percent = 100;
  ledc_set_duty( LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL,
                 percent * ( ( 1 << BACKLIGHT_BITS ) - 1 ) / 100 );
  ledc_update_duty( LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL );
}

static esp_err_t
panel_init( void )
{
  const esp_lcd_dsi_bus_config_t bus_config = {
    .bus_id = 0,
    .num_data_lanes = DSI_LANES,
    .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
    .lane_bit_rate_mbps = DSI_LANE_MBPS,
  };
  ESP_RETURN_ON_ERROR( esp_lcd_new_dsi_bus( &bus_config, &dsi_bus ), TAG,
                       "DSI bus" );

  const esp_lcd_dbi_io_config_t dbi_config = {
    .virtual_channel = 0,
    .lcd_cmd_bits = 8,
    .lcd_param_bits = 8,
  };
  ESP_RETURN_ON_ERROR( esp_lcd_new_panel_io_dbi( dsi_bus, &dbi_config,
                                                 &dbi_io ), TAG, "DBI" );

  /* Elecrow's timing, except the vertical front porch. Elecrow asks for
     51 MHz, but the DPI clock is 240 MHz divided by a whole number
     (mipi_dsi_hal_host_dpi_calculate_divider), so it gets 60 MHz, and
     over 1414 x 645 that's 65.8 Hz. 61 more lines of front porch give
     1414 x 706, 60.10 Hz, against the TS2068's 60.1145 Hz. */
  const esp_lcd_dpi_panel_config_t dpi_config = {
    .virtual_channel = 0,
    .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 60,
    .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs = NUM_FBS,
    .video_timing = {
      .h_size = P4_DISPLAY_WIDTH,
      .v_size = P4_DISPLAY_HEIGHT,
      .hsync_back_porch = 160,
      .hsync_pulse_width = 70,
      .hsync_front_porch = 160,
      .vsync_back_porch = 23,
      .vsync_pulse_width = 10,
      .vsync_front_porch = 73,
    },
  };
  ESP_RETURN_ON_ERROR( esp_lcd_new_panel_dpi( dsi_bus, &dpi_config, &panel ),
                       TAG, "DPI panel" );

  /* The EK79007's start-up, then the DPI video stream */
  ESP_RETURN_ON_ERROR( esp_lcd_panel_io_tx_param( dbi_io, LCD_CMD_SWRESET,
                                                  NULL, 0 ), TAG, "reset" );
  vTaskDelay( pdMS_TO_TICKS( 20 ) );
  ESP_RETURN_ON_ERROR( esp_lcd_panel_io_tx_param( dbi_io, EK79007_PAD_CONTROL,
                         (uint8_t[]){ EK79007_DSI_2_LANE }, 1 ), TAG, "lanes" );
  for( size_t i = 0; i < sizeof( ek79007_init ) / sizeof( ek79007_init[0] );
       i++ ) {
    ESP_RETURN_ON_ERROR( esp_lcd_panel_io_tx_param( dbi_io, ek79007_init[i].cmd,
                           &ek79007_init[i].data, 1 ), TAG, "init" );
  }
  ESP_RETURN_ON_ERROR( esp_lcd_panel_io_tx_param( dbi_io, LCD_CMD_SLPOUT,
                                                  NULL, 0 ), TAG, "wake" );
  vTaskDelay( pdMS_TO_TICKS( 120 ) );

  const esp_lcd_dpi_panel_event_callbacks_t cbs = {
    .on_refresh_done = refresh_done,
  };
  ESP_RETURN_ON_ERROR( esp_lcd_dpi_panel_register_event_callbacks( panel, &cbs,
                                                                   NULL ),
                       TAG, "callbacks" );
  ESP_RETURN_ON_ERROR( esp_lcd_panel_init( panel ), TAG, "DPI start" );
  return esp_lcd_dpi_panel_get_frame_buffer( panel, NUM_FBS, (void **)&fb[0],
                                             (void **)&fb[1], (void **)&fb[2] );
}

esp_err_t
p4_display_init( void )
{
  if( panel ) return ESP_OK;

  const esp_ldo_channel_config_t ldo = {
    .chan_id = DSI_PHY_LDO_CHANNEL,
    .voltage_mv = DSI_PHY_LDO_MV,
  };
  ESP_RETURN_ON_ERROR( esp_ldo_acquire_channel( &ldo, &phy_ldo ), TAG,
                       "DSI PHY LDO" );
  ESP_RETURN_ON_ERROR( backlight_init(), TAG, "backlight" );
  esp_err_t err = panel_init();
  if( err != ESP_OK ) {
    panel = NULL;
    return err;
  }

  for( int i = 0; i < 2; i++ ) {
    strip[i] = heap_caps_aligned_alloc( 64, TV_WIDTH * STRIP_LINES * 2,
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA );
    strip_free[i] = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE( strip[i] && strip_free[i], ESP_ERR_NO_MEM, TAG,
                         "strip" );
    xSemaphoreGive( strip_free[i] );
  }
  const ppa_client_config_t ppa_config = {
    .oper_type = PPA_OPERATION_SRM,
    .max_pending_trans_num = 4,         /* two strips, two copies each */
  };
  ESP_RETURN_ON_ERROR( ppa_register_client( &ppa_config, &ppa ), TAG, "PPA" );
  const ppa_event_callbacks_t ppa_cbs = { .on_trans_done = strip_copied };
  ESP_RETURN_ON_ERROR( ppa_client_register_event_callbacks( ppa, &ppa_cbs ),
                       TAG, "PPA callbacks" );

  size_t fb_bytes = P4_DISPLAY_WIDTH * P4_DISPLAY_HEIGHT * 2;
  for( int i = 0; i < NUM_FBS; i++ ) {
    memset( fb[i], 0, fb_bytes );
    esp_cache_msync( fb[i], fb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M );
    /* No line hashes to 0 (the hash is never 0), so the first frame
       draws every line */
    memset( fb_lines[i], 0, sizeof( fb_lines[i] ) );
  }
  for( int i = 0; i < 256; i++ ) {
    palette2[i] = palette[ i & 0x0f ] | ( palette[ i >> 4 ] << 16 );
  }
  flipped = scanning = 0;               /* the driver starts on fb[0] */
  memset( &stats, 0, sizeof( stats ) );
  p4_display_backlight( 80 );
  return ESP_OK;
}

/* A line's hash: never 0, so a cleared fb_lines entry never matches */
static uint32_t
line_hash( const uint8_t *s )
{
  const uint32_t *w = (const uint32_t *)s;
  uint32_t h = 0x811c9dc5;
  for( int i = 0; i < TV_WIDTH / 4; i++ ) h = ( h ^ w[i] ) * 0x9e3779b1;
  return h | 1;
}

/* Copy strip k, holding n lines from TS2068 line y0, into frame buffer t:
   once to the even rows, once to the odd. The second copy gives the
   strip back when it's done. */
static void
copy_strip( int k, int n, int y0, int t )
{
  ppa_srm_oper_config_t op = {
    .in = {
      .buffer = strip[k],
      .pic_w = TV_WIDTH, .pic_h = n,
      .block_w = TV_WIDTH, .block_h = n,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
    },
    .out = {
      .buffer = fb[t],
      .buffer_size = P4_DISPLAY_WIDTH * P4_DISPLAY_HEIGHT * 2,
      .pic_w = 2 * P4_DISPLAY_WIDTH, .pic_h = P4_DISPLAY_HEIGHT / 2,
      .block_offset_x = P4_DISPLAY_TV_X,
      .block_offset_y = P4_DISPLAY_TV_Y / 2 + y0,
      .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
    },
    .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
    .scale_x = 1.0, .scale_y = 1.0,
    .mode = PPA_TRANS_MODE_NON_BLOCKING,
  };
  ppa_do_scale_rotate_mirror( ppa, &op );
  op.out.block_offset_x += P4_DISPLAY_WIDTH;
  op.user_data = strip_free[k];
  ppa_do_scale_rotate_mirror( ppa, &op );
  stats.strips++;
}

int
p4_display_ts2068( p4_display_line_fn render_line )
{
  if( !panel ) return 1;

  int64_t t0 = esp_timer_get_time();
  int f = flipped, s = scanning;

  if( redraw_all ) {
    for( int i = 0; i < NUM_FBS; i++ ) memset( fb_lines[i], 0, sizeof( fb_lines[i] ) );
  }

  /* Draw into the buffer that's neither scanned nor waiting. The
     interrupt can only make `scanning` equal `flipped`, so it can't
     become this one. Only the lines this buffer doesn't already hold. */
  int t = 0;
  while( t == f || t == s ) t++;

  bool changed = false;                 /* from the frame last shown */
  int first = TV_HEIGHT, last = -1, converted = 0;
  int k = 0, n = 0, y0 = 0;             /* the strip being filled */
  for( int y = 0; y < TV_HEIGHT; y++ ) {
    render_line( y, line_buf );
    uint32_t h = line_hash( line_buf );
    if( h != fb_lines[f][y] ) changed = true;
    if( h == fb_lines[t][y] ) {
      if( n ) { copy_strip( k, n, y0, t ); k ^= 1; n = 0; }
      continue;
    }
    if( n == STRIP_LINES ) { copy_strip( k, n, y0, t ); k ^= 1; n = 0; }
    if( n == 0 ) {                      /* until the PPA has copied it */
      xSemaphoreTake( strip_free[k], portMAX_DELAY );
      y0 = y;
    }
    const uint16_t *src = (const uint16_t *)line_buf;
    uint32_t *d = (uint32_t *)( strip[k] + n * TV_WIDTH );
    for( int x = 0; x < TV_WIDTH / 2; x++ ) {
      uint16_t two = src[x];            /* little-endian: pixel x*2 low */
      d[x] = palette2[ ( two & 0x0f ) | ( ( two >> 4 ) & 0xf0 ) ];
    }
    n++;
    fb_lines[t][y] = h;
    converted++;
    if( first > y ) first = y;
    last = y;
  }
  if( n ) copy_strip( k, n, y0, t );
  for( int i = 0; i < 2; i++ ) {        /* wait for the PPA */
    xSemaphoreTake( strip_free[i], portMAX_DELAY );
    xSemaphoreGive( strip_free[i] );
  }
  if( side && side( fb[t], t, f ) ) changed = true;
  if( !changed ) {
    stats.unchanged++;
    return 1;
  }
  if( f != s ) stats.replaced++;        /* the last flip never got shown */
  if( last < 0 ) first = last = 0;      /* already held: just flip */

  /* A pointer inside one of the panel's own frame buffers: no copy, and
     that buffer is scanned from the next refresh. (Its cache write-back
     finds nothing to write: the PPA wrote these rows, not the CPU.) */
  int64_t t1 = esp_timer_get_time();
  esp_lcd_panel_draw_bitmap( panel, P4_DISPLAY_TV_X, P4_DISPLAY_TV_Y + 2 * first,
                             P4_DISPLAY_TV_X + TV_WIDTH,
                             P4_DISPLAY_TV_Y + 2 * last + 2,
                             fb[t] + ( P4_DISPLAY_TV_Y + 2 * first ) * P4_DISPLAY_WIDTH
                             + P4_DISPLAY_TV_X );
  flipped = t;

  int64_t t2 = esp_timer_get_time();
  if( t2 - t1 > stats.flip_us_max ) stats.flip_us_max = t2 - t1;
  uint32_t dt = t2 - t0;
  stats.drawn++;
  stats.lines += converted;
  stats.draw_us_total += dt;
  if( dt > stats.draw_us_max ) stats.draw_us_max = dt;
  return 0;
}

void
p4_display_get_stats( p4_display_stats_t *s )
{
  *s = stats;
  s->refreshes = refreshes;
}

void
p4_display_set_side( p4_display_side_fn fn )
{
  side = fn;
}

void
p4_display_redraw_all( int on )
{
  redraw_all = on;
}

int
p4_display_check( p4_display_line_fn render_line )
{
  if( !panel ) return -1;
  const uint16_t *f = fb[ flipped ];
  int bad = 0;
  esp_cache_msync( (void *)f, P4_DISPLAY_WIDTH * P4_DISPLAY_HEIGHT * 2,
                   ESP_CACHE_MSYNC_FLAG_DIR_M2C );
  for( int y = 0; y < 2 * TV_HEIGHT; y++ ) {
    if( !( y & 1 ) ) render_line( y / 2, line_buf );
    const uint16_t *row = f + ( P4_DISPLAY_TV_Y + y ) * P4_DISPLAY_WIDTH
                          + P4_DISPLAY_TV_X;
    for( int x = 0; x < TV_WIDTH; x++ ) {
      if( row[x] != palette[ line_buf[x] & 0x0f ] ) bad++;
    }
  }
  return bad;
}
