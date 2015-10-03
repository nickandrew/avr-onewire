/*  vim:sw=4:ts=4:
**  1-wire protocol library for ATTiny85
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#ifndef _ONEWIRE_H_
#define _ONEWIRE_H_

#include <stdint.h>

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
**  GAP_I    Reset devices release bus (divided by 8)
**  GAP_J    Reset devices delay after sampling (divided by 8)
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
	OW0_IDLE,         // Bus is currently idle or timeslot still finishing
	OW0_START,        // Next interrupt begins a timeslot
	OW0_READWAIT,     // Wait 9us before sampling bus
	OW0_SAMPLE,       // Sample bus then go to idle for 55us
	OW0_RELEASE,      // Pullup bus then go to idle for 10us
	OW0_RESET,        // Pulldown bus for 480us to reset all devices
	OW0_RESET1,
	OW0_RESET2,
	OW0_RESET3,
	OW0_DELAY1US,     // Start delay countdown at 1 us per timer count
	OW0_DELAY8US,     // Start delay countdown at 8 us per timer count
	OW0_DELAY128US,   // Start delay countdown at 128 us per timer count
	OW0_DELAY,
	OW0_DELAY_END,    // Reset the timer after an 8 us or 128 us delay operation

	// For conversion delays
	OW0_CONVERT,      // Start 750ms delay with strong pullup
	OW0_CONVERT_DELAY,
};

enum onewire0_process {
	OW0_PIDLE,
};

struct onewire {
	volatile enum onewire0_state state;
	volatile uint8_t current_byte;
	volatile uint8_t bit_id;
	volatile enum onewire0_process process;
	volatile uint8_t ocr0a;
	volatile uint16_t delay_count;
};

struct onewire_id {
	uint8_t device_id[8];
};

// These are for the device ID search algorithm

struct onewire_search {
	volatile uint8_t device_id[8];
	volatile uint8_t last_discrepancy;
	volatile uint8_t last_family_discrepancy;
	volatile uint8_t last_device_flag;
	volatile uint8_t last_zero;
};

struct onewire_scratchpad {
	uint8_t temp_lsb;
	uint8_t temp_msb;
	uint8_t t_h;
	uint8_t t_l;
	uint8_t config;
	uint8_t reserved_1;
	uint8_t reserved_2;
	uint8_t reserved_3;
	uint8_t crc;
};

extern void    onewire0_init(void);
extern void    onewire0_poll(void);
extern uint8_t onewire0_readbyte(void);
extern uint8_t onewire0_reset(void);
extern uint8_t onewire0_search(void);
extern void    onewire0_writebyte(uint8_t byte);
extern uint8_t onewire0_isidle(void);
extern uint8_t onewire0_state(void);

// Delay functions
extern void    onewire0_convertdelay(void);
extern void    onewire0_delay1(uint8_t ocr0a, uint16_t usec1);
extern void    onewire0_delay8(uint8_t ocr0a, uint16_t usec8);
extern void    onewire0_delay128(uint8_t ocr0a, uint16_t usec128);

// OneWire high level functions

extern void    onewire0_readrom(struct onewire_id *buf);
extern void    onewire0_matchrom(struct onewire_id *buf);
extern void    onewire0_skiprom(void);
extern void    onewire0_convert(void);
extern void    onewire0_readscratchpad(void);
extern uint8_t onewire0_get_family_code(struct onewire_id *dev);
extern uint8_t onewire0_check_crc(uint8_t *cp, uint8_t length);

#endif
