/*  vim:sw=4:ts=4:
**  8-bit CRC calculation for 1-wire devices
*/

#ifndef _MAXIM_CRC8_H
#define _MAXIM_CRC8_H

#include <stdint.h>

extern uint8_t crc8_update(uint8_t crc, uint8_t data);

#endif
