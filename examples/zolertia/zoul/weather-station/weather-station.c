/*
 * Copyright (c) 2016, Antonio Lignan - antonio.lignan@gmail.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup weather-station
 * @{
 *
 * \file
 * Weather station application
 *
 * \author
 *         Antonio Lignan <antonio.lignan@gmail.com>
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "lib/random.h"
#include "sys/etimer.h"
#include "dev/bmpx8x.h"
#include "dev/sht25.h"
#include "dev/weather-meter.h"
#include "weather-station.h"
#include "dev/leds.h"
#include "cfs/cfs.h"
#include "cfs/cfs-coffee.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "httpd-simple.h"
/*---------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
weather_station_t weather_sensor_values;
/*---------------------------------------------------------------------------*/
process_event_t weather_station_started_event;
process_event_t weather_station_data_event;
/*---------------------------------------------------------------------------*/
weather_station_config_t ws_config;
/*---------------------------------------------------------------------------*/
PROCESS(weather_station_process, "Weather Station process");
/*---------------------------------------------------------------------------*/
static void
rain_callback(uint16_t value)
{
  PRINTF("WS: *** Rain gauge over threshold (%u ticks)\n", value);
  weather_meter.configure(WEATHER_METER_RAIN_GAUGE_INT_OVER,
                          (value + RAIN_GAUGE_THRESHOLD_TICK));
}
/*---------------------------------------------------------------------------*/
static void
wind_speed_callback(uint16_t value)
{
  PRINTF("WS: *** Wind speed over threshold (%u ticks)\n", value);
}
/*---------------------------------------------------------------------------*/
static void
poll_sensors(void)
{
  weather_sensor_values.counter++;

  /* Poll the weather meter */
  weather_sensor_values.rain_mm = weather_meter.value(WEATHER_METER_RAIN_GAUGE);
  weather_sensor_values.wind_speed = weather_meter.value(WEATHER_METER_ANEMOMETER);
  weather_sensor_values.wind_dir = weather_meter.value(WEATHER_METER_WIND_VANE);
  weather_sensor_values.wind_dir_avg_int = weather_meter.value(WEATHER_METER_WIND_VANE_AVG_X);
  weather_sensor_values.wind_speed_avg = weather_meter.value(WEATHER_METER_ANEMOMETER_AVG);
  weather_sensor_values.wind_speed_avg_int = weather_meter.value(WEATHER_METER_ANEMOMETER_AVG_X);
  weather_sensor_values.wind_speed_max = weather_meter.value(WEATHER_METER_ANEMOMETER_MAX);

  /* Poll the atmospheric pressure sensor */
  SENSORS_ACTIVATE(bmpx8x);
  weather_sensor_values.atmospheric_pressure = bmpx8x.value(BMPx8x_READ_PRESSURE);
  SENSORS_DEACTIVATE(bmpx8x);

  /* Poll the temperature and humidity sensor */
  SENSORS_ACTIVATE(sht25);
  weather_sensor_values.temperature = sht25.value(SHT25_VAL_TEMP);
  weather_sensor_values.humidity = sht25.value(SHT25_VAL_HUM);
  SENSORS_DEACTIVATE(sht25);

  /* Check if the I2C-based sensor values are OK */
  if(weather_sensor_values.atmospheric_pressure == BMPx8x_ERROR) {
    PRINTF("WS: *** BMPx8x failed, sending zero instead\n");
    weather_sensor_values.atmospheric_pressure = 0;
  }

  if((weather_sensor_values.temperature == SHT25_ERROR) ||
    (weather_sensor_values.humidity == SHT25_ERROR)) {
    PRINTF("WS: *** SHT25 failed, sending zero instead\n");
    weather_sensor_values.temperature = 0;
    weather_sensor_values.humidity = 0;
  }

  PRINTF("WS: Pressure = %u.%u(hPa)\n", (weather_sensor_values.atmospheric_pressure / 10),
                                    (weather_sensor_values.atmospheric_pressure % 10));

  PRINTF("WS: Temperature %02d.%02d ºC, ", (weather_sensor_values.temperature / 100),
                                       (weather_sensor_values.temperature % 100));

  PRINTF("Humidity %02d.%02d RH\n", (weather_sensor_values.humidity / 100),
                                    (weather_sensor_values.humidity % 100));

  PRINTF("WS: Rain (ticks): %u, ", weather_sensor_values.rain_mm);

  PRINTF("Wind dir: %u.%01u deg, ", (weather_sensor_values.wind_dir / 10),
                                    (weather_sensor_values.wind_dir % 10));
  PRINTF("(%u.%01u deg avg)\n", (weather_sensor_values.wind_dir_avg_int / 10),
                                (weather_sensor_values.wind_dir_avg_int % 10));

  PRINTF("WS: Wind speed: %u m/h ", weather_sensor_values.wind_speed);
  PRINTF("(%u m/h avg, %u m/h 2m avg, %u m/h max)\n\n", weather_sensor_values.wind_speed_avg,
                                                        weather_sensor_values.wind_speed_avg_int,
                                                        weather_sensor_values.wind_speed_max);

  /* Post the event */
  process_post(PROCESS_BROADCAST, weather_station_data_event, NULL);
}
/*---------------------------------------------------------------------------*/
static int
interval_post_handler(char *key, int key_len, char *val, int val_len)
{
  int fd;
  int rv = 0;
  uint8_t buf[2];

  if(key_len != strlen("interval") ||
     strncasecmp(key, "interval", strlen("interval")) != 0) {
    return HTTPD_SIMPLE_POST_HANDLER_UNKNOWN;
  }

  rv = atoi(val);

  if(rv < WEATHER_STATION_WS_INTERVAL_MIN ||
     rv > WEATHER_STATION_WS_INTERVAL_MAX) {
    return HTTPD_SIMPLE_POST_HANDLER_ERROR;
  }

  ws_config.interval = rv * CLOCK_SECOND;
  PRINTF("WS: new interval tick is: %u\n", rv);

  fd = cfs_open("WS_int", CFS_READ | CFS_WRITE);
  if(fd >= 0) {
    buf[0] = ((uint8_t *)&ws_config.interval)[0];
    buf[1] = ((uint8_t *)&ws_config.interval)[1];
    if(cfs_write(fd, &buf, 2) > 0) {
      PRINTF("WS: interval saved in flash\n");
    }
    cfs_close(fd);
  }

  return HTTPD_SIMPLE_POST_HANDLER_OK;
}
/*---------------------------------------------------------------------------*/
HTTPD_SIMPLE_POST_HANDLER(interval, interval_post_handler);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(weather_station_process, ev, data)
{
  static int fd;
  static uint8_t buf[2];
  static struct etimer et;

  PROCESS_BEGIN();

  weather_station_started_event = process_alloc_event();
  weather_station_data_event = process_alloc_event();

  /* Register the weather meter callback handlers when thresholds are met */
  WEATHER_METER_REGISTER_ANEMOMETER_INT(wind_speed_callback);
  WEATHER_METER_REGISTER_RAIN_GAUGE_INT(rain_callback);

  /* Activate the sensors */
  SENSORS_ACTIVATE(weather_meter);

  /* Configure interruption and events */
  weather_meter.configure(WEATHER_METER_ANEMOMETER_INT_OVER,
                          ANEMOMETER_THRESHOLD_TICK);
  weather_meter.configure(WEATHER_METER_RAIN_GAUGE_INT_OVER,
                          RAIN_GAUGE_THRESHOLD_TICK);

  /* Post the event to notify subscribers */
  process_post(PROCESS_BROADCAST, weather_station_started_event, NULL);

  /* Start the webserver */
  process_start(&httpd_simple_process, NULL);

  /* Read configuration from flash */
  ws_config.interval = WEATHER_STATION_SENSOR_PERIOD;
  fd = cfs_open("WS_int", CFS_READ | CFS_WRITE);
  if(fd >= 0) {
    if(cfs_read(fd, &buf, 2) > 0) {
      ws_config.interval = (buf[1] << 8) + buf[0];
    }
    cfs_close(fd);
  }

  PRINTF("WS: interval %u\n", (uint16_t)(ws_config.interval / CLOCK_SECOND));

  /* The HTTPD_SIMPLE_POST_HANDLER macro should have already created the
   * interval_handler()
   */
  httpd_simple_register_post_handler(&interval_handler);

  /* Start the periodic process */
  etimer_set(&et, ws_config.interval);

  weather_sensor_values.counter = 0;

  while(1) {

    PROCESS_YIELD();

    if(ev == httpd_simple_event_new_config) {
      PRINTF("WS: New configuration over httpd, restarting timer\n");
      etimer_stop(&et);
      etimer_set(&et, ws_config.interval);
    }

    if(ev == PROCESS_EVENT_TIMER && data == &et) {
      poll_sensors();
      etimer_set(&et, ws_config.interval);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/** @} */
