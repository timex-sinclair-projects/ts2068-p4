/* p4_touch.c: the CrowPanel Advanced 7"'s GT911 touch panel (p4_touch.h).

   The bus pins, the address and the panel's orientation (no swap, no
   mirror) come from Elecrow's Lesson05 example (peripheral/bsp_i2c and
   bsp_display), not its code (third_party/PROVENANCE.md, "Facts taken
   from other sources"); the product ID and resolution were read back
   from the chip. The registers are the GT911's: a 16-bit address, sent
   high byte first; 0x8140 the product ID; 0x814E the status (bit 7: a
   new report, bits 0-3: how many touches), which is written back to 0
   to take the report; from 0x814F, 8 bytes a touch (track ID, X, Y and
   size, little-endian, and one reserved). No reset: the chip comes up at
   0x5D on its own. */

#include <string.h>

#include "driver/i2c_master.h"
#include "esp_timer.h"

#include "p4_touch.h"

#define TOUCH_PORT     1
#define TOUCH_GPIO_SDA 45
#define TOUCH_GPIO_SCL 46
#define TOUCH_ADDR     0x5d
#define TOUCH_HZ       400000
#define TOUCH_TIMEOUT_MS 50              /* in ticks, so not under 10 ms */

#define GT911_PRODUCT_ID 0x8140
#define GT911_STATUS     0x814e
#define GT911_POINTS     0x814f
#define GT911_READY      0x80

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;
static p4_touch_point_t last[ P4_TOUCH_MAX ];
static int last_count;
static p4_touch_stats_t stats;

static esp_err_t
gt911_read( uint16_t reg, uint8_t *buf, size_t len )
{
  uint8_t addr[2] = { reg >> 8, reg & 0xff };
  return i2c_master_transmit_receive( dev, addr, 2, buf, len,
                                      TOUCH_TIMEOUT_MS );
}

static esp_err_t
gt911_write_byte( uint16_t reg, uint8_t value )
{
  uint8_t b[3] = { reg >> 8, reg & 0xff, value };
  return i2c_master_transmit( dev, b, 3, TOUCH_TIMEOUT_MS );
}

esp_err_t
p4_touch_init( void )
{
  esp_err_t err;
  uint8_t id[4];

  if( dev ) return ESP_OK;

  const i2c_master_bus_config_t bus_cfg = {
    .i2c_port = TOUCH_PORT,
    .sda_io_num = TOUCH_GPIO_SDA,
    .scl_io_num = TOUCH_GPIO_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };
  if( ( err = i2c_new_master_bus( &bus_cfg, &bus ) ) != ESP_OK ) return err;
  const i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = TOUCH_ADDR,
    .scl_speed_hz = TOUCH_HZ,
  };
  if( ( err = i2c_master_bus_add_device( bus, &dev_cfg, &dev ) ) != ESP_OK ||
      ( err = gt911_read( GT911_PRODUCT_ID, id, 4 ) ) != ESP_OK ) {
    goto fail;
  }
  if( memcmp( id, "911", 3 ) ) { err = ESP_ERR_NOT_FOUND; goto fail; }
  return ESP_OK;

fail:
  if( dev ) i2c_master_bus_rm_device( dev );
  i2c_del_master_bus( bus );
  dev = NULL;
  bus = NULL;
  return err;
}

int
p4_touch_read( p4_touch_point_t *points, int max )
{
  int64_t t0 = esp_timer_get_time();
  uint8_t status, raw[ 8 * P4_TOUCH_MAX ];

  if( !dev ) return 0;
  stats.reads++;
  if( gt911_read( GT911_STATUS, &status, 1 ) != ESP_OK ) goto error;
  if( status & GT911_READY ) {
    int n = status & 0x0f;
    if( n > P4_TOUCH_MAX ) n = P4_TOUCH_MAX;
    if( n && gt911_read( GT911_POINTS, raw, 8 * n ) != ESP_OK ) goto error;
    if( gt911_write_byte( GT911_STATUS, 0 ) != ESP_OK ) goto error;
    for( int i = 0; i < n; i++ ) {
      last[i].x = raw[ 8 * i + 1 ] | raw[ 8 * i + 2 ] << 8;
      last[i].y = raw[ 8 * i + 3 ] | raw[ 8 * i + 4 ] << 8;
    }
    last_count = n;
    stats.reports++;
  }
  goto done;

error:
  stats.errors++;
  last_count = 0;

done:;
  int n = last_count < max ? last_count : max;
  memcpy( points, last, n * sizeof( *points ) );
  uint32_t dt = esp_timer_get_time() - t0;
  if( dt > stats.read_us_max ) stats.read_us_max = dt;
  return n;
}

void
p4_touch_get_stats( p4_touch_stats_t *s )
{
  *s = stats;
}
