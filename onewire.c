/*  vim:sw=4:ts=4:
**  1-wire protocol library for ATTiny85
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#include <avr/io.h>
#include <avr/interrupt.h>

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

struct onewire onewire0;

void onewire0_init(void)
{
	onewire0.state = OW0_IDLE;
	onewire0.process = OW0_PIDLE;

	// Setup I/O pin, initial tri-state, when enabled output low
	DDRB &= ~( PIN );
	PORTB &= ~( PIN );

	// Setup timer0
	// GTCCR |= ( 1<<TSM );  // Disable the timer for programming
	// GTCCR |= ( 1<<PSR0 );
	// Enable CTC mode
	TCCR0A |= ( 1<<WGM01 );
	TCCR0B &= ~PRESCALER;
	OCR0B = 0xff; // Not used
	TIMSK |= 1<<OCIE0A;
}

static void _starttimer(void)
{
	// Clear any pending timer interrupt
	TIFR |= 1<<OCF0A;
	// Start the timer counting
	TCNT0 = 0;
	TCCR0B |= PRESCALER;
}

static void _release(void)
{
	DDRB &= ~( PIN );
}

static void _pulllow(void)
{
	DDRB |= PIN;
}

static void _writebit(uint8_t value)
{
	if (onewire0.state == OW0_IDLE) {
		TCNT0 = 0;
		// Wait 4us until starting the bit
		OCR0A = 4;
	}
	else if (onewire0.state != OW0_WAIT) {
		// Wait until it hits OW0_WAIT
		while (onewire0.state != OW0_WAIT) { };
	}

	if (value) {
		onewire0.state = OW0_WRITE1_A;
	} else {
		onewire0.state = OW0_WRITE0_A;
	}
}

static uint8_t _readbit(void)
{
	if (onewire0.state == OW0_IDLE) {
		TCNT0 = 0;
		// Wait 4us until starting the bit
		OCR0A = 4;
	}
	else if (onewire0.state != OW0_WAIT) {
		// Wait until it hits OW0_WAIT
		while (onewire0.state != OW0_WAIT) { };
	}

	onewire0.state = OW0_READ_A;

	while (onewire0.state != OW0_IDLE) { };

	return onewire0.bit & 0x80;
}

void onewire0_writebyte(uint8_t byte)
{
	uint8_t bit;

	_starttimer();

	for (bit = 0; bit < 8; bit++) {
		_writebit(byte & 1);
		while (onewire0.state != OW0_IDLE) { };
		byte >>= 1;
	}
}

void onewire0_writebyte2(uint8_t byte)
{
	// Make sure all work is done before continuing
	while (onewire0.state != OW0_IDLE) { }

	onewire0.bit_id = 0;
	onewire0.byte_out = byte;
	_writebit(byte & 0);
}

uint8_t onewire0_readbyte(void)
{
	uint8_t bit;

	_starttimer();

	// Wait until device is idle
	while (onewire0.state != OW0_IDLE) { }

	for (bit = 0; bit < 8; bit++) {
		onewire0.state = OW0_READ_A;
		while (onewire0.state != OW0_WAIT) { }
	}

	return onewire0.bit;
}

uint8_t onewire0_readbyte2(void) {
	_starttimer();

	// Wait until device is idle
	while (onewire0.state != OW0_IDLE) { }

	onewire0.process = OW0_READBYTE;
	onewire0.state = OW0_READ_A;

	// Wait until byte completely read
	while (onewire0.state != OW0_IDLE) { }

	return onewire0.bit;
}

// Poll the current state. If OW0_IDLE, that means a read or write
// bit has finished, and so do whatever is next in our process.

void onewire0_poll(void)
{
	if (onewire0.state != OW0_IDLE) {
		// Still going
		return;
	}

	switch(onewire0.process) {
		case OW0_PIDLE:
			// Do nothing
			break;

		case OW0_WRITEBYTE:
			if (onewire0.bit_id < 7) {
				onewire0.byte_out >>= 1;
				onewire0.bit_id ++;
				_writebit(onewire0.byte_out & 0);
			} else {
				onewire0.process = OW0_PIDLE;
			}
			break;

		case OW0_READBYTE:
			if (onewire0.bit_id < 7) {
				onewire0.bit_id ++;
				_readbit();
			} else {
				// Answer is in onewire0.bit
				onewire0.process = OW0_PIDLE;
			}
			break;
	}
}

// Interrupt routine for timer0, OCR0A

ISR(TIMER0_COMPA_vect)
{
	switch(onewire0.state) {
		case OW0_IDLE:
			// Do nothing
			break;

		case OW0_WRITE0_A:
			_pulllow();
			OCR0A = GAP_C;
			onewire0.state = OW0_WRITE0_B;
			break;

		case OW0_WRITE0_B:
			_release();
			OCR0A = GAP_D;
			onewire0.state = OW0_WAIT;
			break;

		case OW0_WRITE1_A:
			_pulllow();
			OCR0A = GAP_A;
			onewire0.state = OW0_WRITE1_B;
			break;

		case OW0_WRITE1_B:
			_release();
			OCR0A = GAP_B;
			onewire0.state = OW0_WAIT;
			break;

		case OW0_READ_A:
			_pulllow();
			OCR0A = GAP_A;
			onewire0.state = OW0_READ_B;
			break;

		case OW0_READ_B:
			_release();
			OCR0A = GAP_E;
			onewire0.state = OW0_READ_C;
			break;

		case OW0_READ_C:
			// Bits are read from 0 to 7, which means we
			// have to shift previous contents and store current
			// input into bit 7
			onewire0.bit = (onewire0.bit >> 1) | ((PINB & (PIN)) ? 0x80 : 0);
			OCR0A = GAP_F;
			onewire0.state = OW0_WAIT;
			break;

		case OW0_WAIT:
			// This will ensure the interrupt is re-triggered every 20us
			// in idle state.
			OCR0A = 20;
			onewire0.state = OW0_IDLE;
			break;

		case OW0_DELAY:
			if (! --onewire0.delay_count) {
				onewire0.state = OW0_IDLE;
			}
			break;
	}

	if (onewire0.state == OW0_IDLE) {
		// Perform the next action in the meta-process
	}
}
