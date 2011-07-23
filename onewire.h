/*  vim:sw=4:ts=4:
**  1-wire protocol library for ATTiny85
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#ifndef _ONEWIRE_H_
#define _ONEWIRE_H_

#include <stdint.h>

// I/O pin
#define PIN ( 1 << PORTB0 )

#ifndef CPU_FREQ
#define CPU_FREQ 8000000
#endif

#if CPU_FREQ == 8000000
// Prescaler CK/8
#define PRESCALER ( 1<<CS01 )
// When resetting the devices, use 8us resolution
#define RESET_PRESCALER ( 1<<CS01 | 1<<CS00)
#else
#error "Only CPU_FREQ of 8 MHz is presently supported"
#endif

/*
**  1wire signal timing, in microseconds unless otherwise noted
**
**  GAP_A    Write 1 bit bus low and Read bit bus low
**  GAP_B    Write 1 bit release bus
**  GAP_C    Write 0 bit bus low
**  GAP_D    Write 0 bit release bus
**  GAP_E    Read bit release bus
**  GAP_F    Read bit after sampling
**  GAP_G    Reset devices initial delay (zero so not used)
**  GAP_H    Reset devices bus low (divided by 8)
**  GAP_I    Reset devices release bus
**  GAP_J    Reset devices delay after sampling
*/

#define GAP_A  6
#define GAP_B 64
#define GAP_C 60
#define GAP_D 10
#define GAP_E  9
#define GAP_F 55
#define GAP_G  0
#define GAP_H 60
#define GAP_I  9
#define GAP_J 51

enum onewire0_state {
	OW0_IDLE,
	OW0_WRITE0_A,
	OW0_WRITE0_B,
	OW0_WRITE1_A,
	OW0_WRITE1_B,
	OW0_READ_A,
	OW0_READ_B,
	OW0_READ_C,
	OW0_WAIT,
	OW0_DELAY,
};

enum onewire0_process {
	OW0_PIDLE,
	OW0_WRITEBYTE,
	OW0_READBYTE,
};

struct onewire {
	volatile enum onewire0_state state;
	volatile uint8_t bit;
	volatile uint8_t byte_out;
	volatile uint8_t bit_id;
	volatile enum onewire0_process process;
	volatile uint8_t delay_count;
};

extern void onewire0_init(void);

#endif
