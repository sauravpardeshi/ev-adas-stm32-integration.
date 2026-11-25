#include "mpu6050.h"
#include "hal.h"
#include <string.h>
#define REG_ACCEL_CFG 0x1C
#define REG_ACCEL_OUT 0x3B
#define REG_PWR_MGMT  0x6B
#define REG_WHO_AM_I  0x75
#define MPU_RETRIES   3
static uint8_t prev[14]; static uint8_t same;
static uint32_t retries, recovers;

int mpu_init(void){
    uint8_t id = 0, v;
    hal_i2c_bus_recover();                 /* start from a known-good bus */
    if (hal_i2c_read(MPU6050_ADDR, REG_WHO_AM_I, &id, 1) != 0) return MPU_ERR_BUS;
    if (id != 0x68) return MPU_ERR_ID;
    v = 0x80; if (hal_i2c_write(MPU6050_ADDR, REG_PWR_MGMT, &v, 1)) return MPU_ERR_BUS; /* device reset */
    v = 0x00; if (hal_i2c_write(MPU6050_ADDR, REG_PWR_MGMT, &v, 1)) return MPU_ERR_BUS; /* wake         */
    v = 0x08; if (hal_i2c_write(MPU6050_ADDR, REG_ACCEL_CFG, &v, 1)) return MPU_ERR_BUS; /* +/-4 g       */
    memset(prev, 0, sizeof prev); same = 0;
    return MPU_OK;
}

int mpu_read(mpu_sample_t *s){
    uint8_t b[14]; int rc = -1;
    for (int attempt = 0; attempt < MPU_RETRIES; attempt++) {
        rc = hal_i2c_read(MPU6050_ADDR, REG_ACCEL_OUT, b, 14);
        if (rc == 0) break;
        retries++;
        if (attempt == 1) { hal_i2c_bus_recover(); recovers++; }   /* 2nd failure: clear the bus */
    }
    if (rc != 0) return MPU_ERR_BUS;
    /* frozen-data detector: a live MEMS sensor never repeats a whole 14-byte frame */
    if (memcmp(b, prev, 14) == 0) { if (++same >= IMU_FROZEN_N) return MPU_ERR_FROZEN; }
    else { same = 0; memcpy(prev, b, 14); }
    int16_t rx = (int16_t)((b[0]<<8)|b[1]), ry = (int16_t)((b[2]<<8)|b[3]), rz = (int16_t)((b[4]<<8)|b[5]);
    /* +/-4 g => 8192 LSB/g ; mg = raw*1000/8192 = raw*125/1024 */
    s->ax_mg = (int16_t)(((int32_t)rx*125)/1024);
    s->ay_mg = (int16_t)(((int32_t)ry*125)/1024);
    s->az_mg = (int16_t)(((int32_t)rz*125)/1024);
    return MPU_OK;
}
uint32_t mpu_retry_count(void){ return retries; }
uint32_t mpu_recover_count(void){ return recovers; }
