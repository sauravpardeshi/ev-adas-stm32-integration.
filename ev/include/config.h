/* config.h - build options, timing and threshold configuration */
#ifndef CONFIG_H
#define CONFIG_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FW_VERSION "1.0.0"

/* ---- build-time options (used for the A/B integration experiments) ---- */
#ifndef RANGE_BLOCKING
#define RANGE_BLOCKING 0   /* 1 = naive pulseIn()-style blocking HC-SR04 read */
#endif
#ifndef UART_BLOCKING
#define UART_BLOCKING  0   /* 1 = naive polled blocking UART transmit        */
#endif
#ifndef RANGE_FILTER
#define RANGE_FILTER   1   /* 1 = alpha-beta tracker with gating, 2 = median-of-3 + EMA, 0 = raw */
#endif

#ifndef ESC_VOTE
#define ESC_VOTE      1   /* 1 = escalate on 2-of-last-3 samples, 0 = 2 consecutive samples */
#endif

#ifndef DISPLAY_DIRTY
#define DISPLAY_DIRTY  1   /* 1 = write only changed digits, 0 = rewrite all each cycle */
#endif

/* ---- peripherals ---- */
#define MPU6050_ADDR      0x68
#define ADC_CH_SPEED      0
#define ADC_CH_BATT       1
enum { PIN_BUZZER, PIN_LED_ADV, PIN_LED_WARN, PIN_LED_CRIT, PIN_LED_BATT,
       PIN_SPI_CS, PIN_TRIG, PIN_COUNT };

/* ---- task periods (ms) ---- */
#define P_IMU_MS      20
#define P_SPEED_MS    20
#define P_RANGE_MS    50     /* measurement cadence (>= 2 x max echo 23 ms)   */
#define P_RANGE_POLL  5      /* echo-capture polling period                   */
#define P_ADAS_MS     10
#define P_ALERT_MS    10
#define P_BATT_MS     100
#define P_CLI_MS      10
#define P_DISPLAY_MS  100
#define P_TELEM_MS    200
#define P_SUPERV_MS   100

/* ---- ADAS / sensing thresholds ---- */
#define TTC_ADV_MS        3000
#define TTC_WARN_MS       2000
#define TTC_CRIT_MS       1000
#define DIST_CRIT_MM      150
#define MIN_CLOSING_MMPS  150
#define ADAS_MIN_SPEED_DKMH 50      /* 5.0 km/h: below this only proximity alarm */
#define ESC_CONFIRM       2
#define DEESC_CONFIRM     4
#define RANGE_MAX_MM      4000
#define RANGE_MIN_MM      20
#define RANGE_FAR_MM      9999
#define RANGE_ECHO_TIMEOUT_MS 45
#define RANGE_STALE_MS    300
#define TRACK_GATE_MM     300     /* innovation gate for outlier rejection  */
#define TRACK_MAX_REJ     3       /* consecutive rejects before re-acquire  */

#define IMPACT_MG         2500
#define BRAKE_MG          600
#define BRAKE_REARM_MG    300
#define BRAKE_CONFIRM     2
#define IMU_FROZEN_N      25
#define IMU_STALE_MS      100
#define IMU_REINIT_MS     500

#define SOC_LOW_PCT       20
#define SOC_LOW_CLR       23
#define SOC_CRIT_PCT      10
#define SOC_CRIT_CLR      13

#define UART_TX_SIZE      512
#define LINE_MAX          32
#define WDT_TIMEOUT_MS    100
#define TASK_HANG_LIMIT_US 50000
#endif
