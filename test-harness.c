/*  vim:sw=4:ts=4:
**
**  Test the 1wire library - protocol and timing
**
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#include "onewire.h"

struct onewire_id device_id;

// Connect a logic analyzer to these pins for tracing execution and timing
#define DPINA  (1 << PORTB1)
#define DPINB  (1 << PORTB2)
#define DPINC  (1 << PORTB3)

/* Set highest frequency CPU operation
**  Startup frequency is assumed to be 1 MHz
**  8 MHz for the internal clock
*/

void set_cpu_8mhz(void) {
	// Let's wind this sucker up to 8 MHz
	CLKPR = 1<<CLKPCE;
	CLKPR = 0<<CLKPS3 | 0<<CLKPS2 | 0<<CLKPS1 | 0<<CLKPS0;
	// System clock is now 8 MHz
}

// Set all trace pins to output mode
void init_trace(void)
{
	DDRB |= DPINA | DPINB | DPINC;
	PORTB &= ~( DPINA | DPINB | DPINC );
}

void set_a(void) {
	PORTB |= DPINA;
}

void set_b(void) {
	PORTB |= DPINB;
}

void set_c(void) {
	PORTB |= DPINC;
}

void toggle_a(void) {
	PORTB ^= DPINA;
}

void toggle_b(void) {
	PORTB ^= DPINB;
}

void toggle_c(void) {
	PORTB ^= DPINC;
}

void clr_a(void) {
	PORTB &= ~DPINA;
}

void clr_b(void) {
	PORTB &= ~DPINB;
}

void clr_c(void) {
	PORTB &= ~DPINC;
}

int main(void) {
	uint8_t rc;
	uint8_t bytes;

	cli();
	set_cpu_8mhz();

	init_trace();

	onewire0_init();
	sei();

	while (1) {
		toggle_a();

		// set_b();
		rc = onewire0_reset();
		// clr_b();


		onewire0_readrom(&device_id);

		// Start a temperature conversion
		onewire0_skiprom();
		onewire0_convert();
		onewire0_convertdelay();


		// Wait until it is finished
		while (! onewire0_isidle()) { }
		clr_b();
		onewire0_reset();
		onewire0_skiprom();
		set_b();
		onewire0_readscratchpad();
		for (bytes = 0; bytes < 9; ++bytes) {
			onewire0_readbyte();
		}
		clr_b();

		while (! onewire0_isidle()) { }

	}
}

