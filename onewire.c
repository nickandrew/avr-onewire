/*  vim:sw=4:ts=4:
**  1-wire protocol library for ATTiny85
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "onewire.h"

struct onewire onewire0;

static void _starttimer(void)
{
	// Clear any pending timer interrupt
	TIFR |= 1<<OCF0A;
	// Start the timer counting
	TCNT0 = 0;
	TCCR0B |= PRESCALER;
}

static void _slowtimer(void)
{
	TCCR0B = (TCCR0B & 0xf8) | RESET_PRESCALER;
}

static void _fasttimer(void)
{
	TCCR0B = (TCCR0B & 0xf8) | PRESCALER;
}

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
	OCR0A = 20;   // Initial value, to interrupt once every 20us
	OCR0B = 0xff; // Not used
	TIMSK |= 1<<OCIE0A;

	_starttimer();
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
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = value ? 1 : 0;
	onewire0.bit_id = 0;
	onewire0.state = OW0_START;
}

static uint8_t _readbit(void)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = 1; // Write a 1 bit to sample input
	onewire0.bit_id = 0;
	onewire0.state = OW0_START;

	while (onewire0.state != OW0_IDLE) { }

	return (onewire0.current_byte & 0x80) ? 1 : 0;
}

// Start to write 8 bits.
// Wait for device to be idle before starting.
// Do not wait for the bits to be sent.

static void _write8(uint8_t byte)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = byte;
	onewire0.bit_id = 8;
	onewire0.state = OW0_START;
}

// Start to read 8 bits.
// Wait for device to be idle before starting.
// Do not wait for the bits to arrive.

static void _read8(void)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = 0xff; // Write all 1-bits to sample input 8 times
	onewire0.bit_id = 8;
	onewire0.state = OW0_START;
}

/*  void onewire0_writebyte(uint8_t byte)
**
**  Write 8 bits to the bus.
**  This function takes from 505us to 560us depending on idle state.
**  It returns from 10us to 55us before the next time slot starts.
*/

void onewire0_writebyte(uint8_t byte)
{
	_write8(byte);
}

/*  uint8_t onewire0_readbyte()
**
**  Read 8 bits from the bus and return the byte value.
**  This function takes from 505us to 560us depending on idle state.
**  It returns soon after sampling the last data bit, before the
**  end of the last time slot.
*/

uint8_t onewire0_readbyte(void)
{
	_read8();

	while (onewire0.state != OW0_IDLE) { }

	return onewire0.current_byte;
}

/*  void onewire0_reset(void)
**
**  Reset devices on the bus. Wait for the reset process to complete.
**  Return 1 if there are devices on the bus, else 0.
*/

uint8_t onewire0_reset(void)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.state = OW0_RESET;

	while (onewire0.state != OW0_IDLE) { }

	return (onewire0.current_byte & 0x80) ? 0 : 1;
}

/*  void onewire0_poll(void)
**
**  Fast poll function.
**  If the bus is busy, just return.
**  If the bus is idle, then run more protocol if possible.
*/

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
	}
}

// Prepare for the next bit of I/O.
// If we're processing a byte, then go to state OW0_START for the next bit.
// Otherwise, enter idle state upon next interrupt.

static inline void _nextbit(void) {
	// Perform the next action in the meta-process
	if (onewire0.bit_id) {
		// Continue reading/writing a byte with the next bit
		onewire0.state = OW0_START;
		-- onewire0.bit_id;
	} else {
		// The next state will be idle unless mainline code changes it
		// before the next interrupt (e.g. more bytes to send).
		onewire0.state = OW0_IDLE;
	}
}

// Interrupt routine for timer0, OCR0A

ISR(TIMER0_COMPA_vect)
{
	switch(onewire0.state) {
		case OW0_IDLE:
			// Wait 20us until the next interrupt
			OCR0A = 20;
			break;

		case OW0_START:
			_pulllow();
			if (onewire0.current_byte & 1) {
				// Write a 1-bit or read a bit:
				// 6us low, 9us wait, sample, 55us high
				OCR0A = GAP_A;
				onewire0.state = OW0_READWAIT;
			} else {
				// Write a 0-bit
				// 60us low, 10us high
				OCR0A = GAP_C;
				onewire0.state = OW0_RELEASE;
			}

			onewire0.current_byte >>= 1;
			break;

		case OW0_READWAIT:
			// Let the signal go high, wait 9us then sample.
			_release();
			OCR0A = GAP_E;
			onewire0.state = OW0_SAMPLE;
			break;

		case OW0_SAMPLE:
			// Bits are read from 0 to 7, which means we
			// have to shift current_byte down and store in bit 7
			// Shifting is done in state OW0_START so no need to do it again here.
			onewire0.current_byte |= ((PINB & (PIN)) ? 0x80 : 0);
			OCR0A = GAP_F;
			_nextbit();
			break;

		case OW0_RELEASE:
			// Let the signal go high for 10us.
			_release();
			OCR0A = GAP_D;
			_nextbit();
			break;

		case OW0_RESET:
			// Pull the bus down and wait 480us (slow down the prescaler)
			_pulllow();
			OCR0A = GAP_H;
			_slowtimer();
			onewire0.state = OW0_RESET1;
			break;

		case OW0_RESET1:
			// Release the bus, speed up the prescaler and wait for 9us
			_release();
			OCR0A = GAP_I;
			_fasttimer();
			onewire0.state = OW0_RESET2;
			break;

		case OW0_RESET2:
			// Sample the bus, slow the prescaler down again and wait 408us
			onewire0.current_byte |= ((PINB & (PIN)) ? 0x80 : 0);
			OCR0A = GAP_J;
			_slowtimer();
			onewire0.state = OW0_RESET3;
			break;

		case OW0_RESET3:
			// Speed up the prescaler again, go to idle state with 20us between interrupts
			OCR0A = 20;
			_fasttimer();
			onewire0.state = OW0_IDLE;
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

	// Return from interrupt
}
