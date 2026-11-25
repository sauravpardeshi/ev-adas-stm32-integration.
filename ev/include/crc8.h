#ifndef CRC8_H
#define CRC8_H
#include "config.h"
uint8_t crc8(const uint8_t *d, size_t n);   /* poly 0x07, init 0x00 */
#endif
