/*  vim:sw=4:ts=4:
**
**  Test the 1wire library - protocol and timing
**
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#include <avr/io.h>
#include <stdint.h>

#include "onewire.h"

// Connect a logic analyzer to these pins for tracing execution and timing
#define PINA  (1 << PORTB1)
#define PING  (1 << PORTB2)
#define PINC  (1 << PORTB3)

// Set all trace pins to output mode
void init_trace(void)
{
	DDRB |= PINA | PINB | PINC;
	PORTB &= ~( PINA | PINB | PINC );
}

void set_a(void) {
	PORTB |= PINA;
}

void set_b(void) {
	PORTB |= PINB;
}

void set_c(void) {
	PORTB |= PINC;
}

void clr_a(void) {
	PORTB &= ~PINA;
}

void clr_b(void) {
	PORTB &= ~PINB;
}

void clr_c(void) {
	PORTB &= ~PINC;
}

int main(void) {
	uint8_t rc;

	init_trace();

	set_a();
	onewire0_init();

	set_b();
	rc = onewire0_reset();
	clr_b();

	onewire0_writebyte(0x05);

	set_c();
	rc = onewire0_search();
	clr_c();


	clr_a();
	while (1) { }
}

