/*  vim:sw=4:ts=4:
**  8-bit CRC calculation for 1-wire devices
**
**  Code copied from AVR-Libc manual
**  License: http://www.nongnu.org/avr-libc/LICENSE.txt (included by reference)
**
**  See http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
**  Also http://www.maxim-ic.com/appnotes.cfm/appnote_number/27
*/

#include "maxim-crc8.h"

uint8_t crc8_update(uint8_t crc, uint8_t data) {
	uint8_t i;

	crc = crc ^ data;

	for (i = 0; i < 8; ++i) {
		if (crc & 0x01) {
			crc = (crc >> 1) ^ 0x8c;
		} else {
			crc >>= 1;
		}
	}

	return crc;
}
