#ifndef MPU6050_H
#define MPU6050_H
#include "config.h"
typedef struct { int16_t ax_mg, ay_mg, az_mg; } mpu_sample_t;
typedef enum { MPU_OK=0, MPU_ERR_BUS=-1, MPU_ERR_ID=-2, MPU_ERR_FROZEN=-3 } mpu_status_t;
int  mpu_init(void);                 /* WHO_AM_I check, wake, +/-4 g range */
int  mpu_read(mpu_sample_t *s);      /* burst read, retry, bus recovery    */
uint32_t mpu_retry_count(void);
uint32_t mpu_recover_count(void);
#endif
