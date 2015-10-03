/*  vim:sw=4:ts=4:
**
**  Test the 1wire library - delays
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

	cli();
	set_cpu_8mhz();

	init_trace();

	onewire0_init();
	sei();

	while (1) {
		toggle_a();

		onewire0_delay1(20, 1); // 20 us
		toggle_b();

		onewire0_delay1(24, 1); // 25 us
		toggle_b();

		onewire0_delay1(24, 2); // 50 us
		toggle_b();

		onewire0_delay1(24, 3); // 75 us
		toggle_b();

		onewire0_delay1(24, 4); // 100 us
		toggle_b();

		onewire0_delay1(24, 3); // 75 us
		toggle_b();

		onewire0_delay1(24, 5); // 125 us
		toggle_b();

	}
}

