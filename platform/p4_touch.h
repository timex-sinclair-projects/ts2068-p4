/* p4_touch.h: the CrowPanel Advanced 7"'s touch panel, for the on-screen
   keyboard (PLAN.md Phase 3, "Input, stage 1").

   A Goodix GT911 at I2C address 0x5D (SDA GPIO45, SCL GPIO46), reporting
   up to five touches in the panel's own 1024x600 coordinates. It takes
   I2C port 1, so machine.I2C(0) stays free, but nothing else may use
   GPIO45 and GPIO46 while it runs. */

#ifndef P4_TOUCH_H
#define P4_TOUCH_H

#include <stdint.h>

#include "esp_err.h"

#define P4_TOUCH_MAX 5

typedef struct {
  uint16_t x, y;
} p4_touch_point_t;

/* Open the bus and check the GT911's product ID. Once; a later call does
   nothing. */
esp_err_t p4_touch_init( void );

/* The touches down now, up to max of them, into points; returns how
   many. The GT911 has new data about every 10 ms; between reports this
   returns the last one. On an I2C error, returns 0 (nothing down). */
int p4_touch_read( p4_touch_point_t *points, int max );

typedef struct {
  uint32_t reads;               /* p4_touch_read calls */
  uint32_t reports;             /* new reports from the GT911 */
  uint32_t errors;              /* I2C errors */
  uint32_t read_us_max;         /* longest p4_touch_read */
} p4_touch_stats_t;

void p4_touch_get_stats( p4_touch_stats_t *stats );

#endif /* P4_TOUCH_H */
