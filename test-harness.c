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

#include "onewire0.h"

struct onewire_id device_id;

// Connect a logic analyzer to these pins for tracing execution and timing
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
	DDRB |= DPINB | DPINC;
	PORTB &= ~( DPINB | DPINC );
}

void set_b(void) {
	PORTB |= DPINB;
}

void set_c(void) {
	PORTB |= DPINC;
}

void toggle_b(void) {
	PORTB ^= DPINB;
}

void toggle_c(void) {
	PORTB ^= DPINC;
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
		toggle_b();

		rc = onewire0_reset();

		set_c();
		onewire0_readrom(&device_id);
		toggle_c();

		// Issue skip rom command
		onewire0_skiprom();
		toggle_c();

		// Start a temperature conversion
		onewire0_convert();
		toggle_c();

		// Setup a delay
		onewire0_convertdelay();
		toggle_c();


		// Wait until it is finished
		while (! onewire0_isidle()) { }
		toggle_c();

		onewire0_reset();
		toggle_c();

		onewire0_skiprom();
		toggle_c();

		onewire0_readscratchpad();
		toggle_c();

		for (bytes = 0; bytes < 9; ++bytes) {
			onewire0_readbyte();
		}
		toggle_c();

		while (! onewire0_isidle()) { }
		toggle_c();

	}
}
