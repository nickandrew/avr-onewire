/*  vim:sw=4:ts=4:
**  1-wire protocol library for ATTiny85
**  (C) 2011, Nick Andrew <nick@nick-andrew.net>
**  All Rights Reserved.
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#include <stdint.h>

#include "maxim-crc8.h"

/*
** ---------------------------------------------------------------------------
** Pin and speed definitions
** ---------------------------------------------------------------------------
*/

// Specify the single I/O pin
#ifndef PIN
#define PIN ( 1 << PORTB0 )
#endif

#ifndef CPU_FREQ
#define CPU_FREQ 8000000
#endif

#if CPU_FREQ == 8000000
// Prescaler CLKio/8 = 1 us resolution
#define PRESCALER ( 1<<CS01 )
// When resetting the devices, use CLKio/64 (8 us resolution)
#define RESET_PRESCALER ( 0<<CS02 | 1<<CS01 | 1<<CS00 )
// For long delays, use CLKio/1024 (32 us resolution)
#define DELAY_PRESCALER ( 1<<CS02 | 0<<CS01 | 1<<CS00 )
// When device is idle, interrupt every IDLE_DELAY us
#define IDLE_DELAY 20
#else
#error "Only CPU_FREQ of 8 MHz is presently supported"
#endif

#include "onewire.h"

struct onewire onewire0;
struct onewire_search search0;

static void _starttimer(void)
{
	// Setup timer0

	GTCCR |= (1<<TSM | 1<<PSR0);  // Disable the timer for programming

	// Enable CTC mode (mode 2); TCNT0 counts from 0 to OCR0A inclusive
	TCCR0A |= ( 1<<WGM01 );

	// Setup Clock Select = 2 (1<<CS01) for clkIO/8
	TCCR0B = PRESCALER;

	// Start counting from 0
	TCNT0 = 0;

	// Initially, interrupt once every 20us
	OCR0A = IDLE_DELAY - 1;

	// OCR0B is not used
	OCR0B = 0xff;

	// Enable interrupt on Compare Match A
	TIMSK |= ( 1<<OCIE0A );

	// Clear any pending timer interrupt
	TIFR |= ( 1<<OCF0A );

	// Start the timer
	GTCCR &= ~( 1<<TSM );
}

inline void _medtimer(void)
{
	// Halt the counter for a moment to reconfigure
	GTCCR |= ( 1<<TSM | 1<<PSR0 );

	TCCR0B = (TCCR0B & 0xf8) | RESET_PRESCALER;
	// Reset counter, so start counting from the moment the timer is re-enabled
	TCNT0 = 0;
	OCR0A = onewire0.ocr0a;

	// Resume counting
	GTCCR &= ~( 1<<TSM );
}

inline void _delaytimer(void)
{
	// Halt the counter for a moment to reconfigure
	GTCCR |= ( 1<<TSM | 1<<PSR0 );

	TCCR0B = (TCCR0B & 0xf8) | DELAY_PRESCALER;
	// Reset counter, so start counting from the moment the timer is re-enabled
	TCNT0 = 0;
	// 256 counts per interrupt
	OCR0A = 255;

	// Resume counting
	GTCCR &= ~( 1<<TSM );
}

/*
**  If the timer is not already in fast mode (found by checking the
**  prescaler) then halt the timer, reconfigure it in fast mode,
**  reset the counter then resume counting.
**
**  The conditional reset logic is used to ensure that an already-fast
**  timer need not be stopped and restarted, which would introduce a
**  small delay.
*/

inline void _fasttimer(void)
{
	if ((TCCR0B & 0x07) != PRESCALER) {
		// Halt the counter for a moment to reconfigure
		GTCCR |= 1<<TSM | 1<<PSR0;

		TCCR0B = (TCCR0B & 0xf8) | PRESCALER;
		// Reset counter, so start counting from the moment the timer is re-enabled
		TCNT0 = 0;

		// Resume counting
		GTCCR &= ~( 1<<TSM );
	}
}

// Get the value of a bit in a multi-byte array.
// Bit numbers start from 1, as used here:
// http://www.maxim-ic.com/app-notes/index.mvp/id/187

static uint8_t _getbit(volatile uint8_t *cp, uint8_t bit_id)
{
	uint8_t bit_mask;

	bit_id --;
	// Point to the containing byte
	cp += (bit_id >> 3);
	bit_mask = 1 << (bit_id & 0x07);
	if (*cp & bit_mask) {
		return 1;
	}

	return 0;
}

// Set/Clear the value of a bit in a multi-byte array.
// Bit numbers start from 1.

static void _setbit(volatile uint8_t *cp, uint8_t bit_id, uint8_t value)
{
	uint8_t bit_mask;

	bit_id --;
	// Point to the containing byte
	cp += (bit_id >> 3);
	bit_mask = 1 << (bit_id & 0x07);
	if (value) {
		*cp |= bit_mask;
	} else {
		*cp &= ~bit_mask;
	}
}

// Calculate an 8-bit CRC for a 1wire device_id
// Return 1 if correct, 0 if wrong.

uint8_t _checkCRC(volatile uint8_t *cp) {
	return 1;
}

void onewire0_init(void)
{
	onewire0.state = OW0_IDLE;
	onewire0.process = OW0_PIDLE;

	// Setup I/O pin, initial tri-state, when enabled output low
	DDRB &= ~( PIN );
	PORTB &= ~( PIN );
	_starttimer();
}

inline void _release(void)
{
	PORTB &= ~( PIN );  // Disable weak pullup
	DDRB &= ~( PIN );   // Set pin mode to input
}

inline void _pulllow(void)
{
	// PORTB is expected to be low at this point
	DDRB |= PIN;
}

// Set a strong pullup on the 1-wire bus: pin set to output, logic high.
// The pullup is obviously limited to the current sourcing capacity of the ATTiny85 (40mA).
// A call to _pullhigh() must be followed by _release before calling _pulllow().

inline void _pullhigh(void)
{
	PORTB |= PIN;       // Output high
	DDRB |= PIN;        // Set pin mode to output
}

static void _wait(void)
{
	while (onewire0.state != OW0_IDLE) { }
}

static void _writebit(uint8_t value)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = value ? 1 : 0;
	onewire0.bit_id = 1;
	onewire0.state = OW0_START;
}

static uint8_t _readbit(void)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = 1; // Write a 1 bit to sample input
	onewire0.bit_id = 1;
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

// Start to read 2 bits.
// Wait for device to be idle before starting.
// Do not wait for the bits to arrive.

static void _read2(void)
{
	while (onewire0.state != OW0_IDLE) { }

	onewire0.current_byte = 0xff; // Write all 1-bits to sample input 2 times
	onewire0.bit_id = 2;
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

/*  void onewire0_delay1(uint8_t ocr0a, uint16_t usec1)
**
**  Setup a delay for some number of microseconds.
**  The delay = (ocr0a + 1) x usec1 x 1 us.
**  The state will become OW0_IDLE after the programmed delay, but there
**  will be another 20 us until the next interrupt. So take that into
**  consideration.
**
**  ocr0a = 0..255 as the number of counts per interrupt (minus 1)
**    Values of ocr0a less than 10 or so won't work, as the interrupt handler
**    takes around 10 us to process each interrupt.
**  usec1 = 0..65535 as the number of interrupts (0 means 65536).
**
**  Maximum sleep time = 65536 x 256 x 1 us = 16.777216 seconds.
*/

void onewire0_delay1(uint8_t ocr0a, uint16_t usec1) {
	while (onewire0.state != OW0_IDLE) { }

	onewire0.ocr0a = ocr0a;
	onewire0.delay_count = usec1;
	onewire0.state = OW0_DELAY1US;
}

/*  void onewire0_delay8(uint8_t ocr0a, uint16_t usec8)
**
**  Setup a delay for some number of microseconds.
**  The delay = (ocr0a + 1) x usec8 x 8 us.
**  The state will become OW0_IDLE after the programmed delay, but there
**  will be another 20 us until the next interrupt. So take that into
**  consideration.
**
**  ocr0a = 0..255 as the number of counts per interrupt (minus 1)
**    Values of ocr0a less than 2 won't work, as the interrupt handler
**    takes around 10 us to process each interrupt.
**  usec8 = 0..65535 as the number of interrupts (0 means 65536).
**
**  Maximum sleep time = 65536 x 256 x 8 us = ~134 seconds.
*/

void onewire0_delay8(uint8_t ocr0a, uint16_t usec8) {
	while (onewire0.state != OW0_IDLE) { }

	onewire0.ocr0a = ocr0a;
	onewire0.delay_count = usec8;
	onewire0.state = OW0_DELAY8US;
}

/*  void onewire0_delay128(uint8_t ocr0a, uint16_t usec128)
**
**  Setup a delay for some number of microseconds.
**  The delay = (ocr0a + 1) x usec128 x 128 us.
**  The state will become OW0_IDLE after the programmed delay, but there
**  will be another 20 us until the next interrupt. So take that into
**  consideration.
**
**  ocr0a = 0..255 as the number of counts per interrupt (minus 1)
**  usec128 = 0..65535 as the number of interrupts (0 means 65536).
**
**  Maximum sleep time = 65536 x 256 x 128 us = ~2147 seconds.
*/

void onewire0_delay128(uint8_t ocr0a, uint16_t usec128) {
	while (onewire0.state != OW0_IDLE) { }

	onewire0.ocr0a = ocr0a;
	onewire0.delay_count = usec128;
	onewire0.state = OW0_DELAY128US;
}

// Reset search
static inline void _resetsearch(void)
{
		search0.last_discrepancy = 0;
		search0.last_family_discrepancy = 0;
		search0.last_device_flag = 0;
}

/*  uint8_t onewire0_search(void)
**
**  Initiate a 1wire device number search algorithm,
**  and find the first 64-bit address.
**
**  Return 1 if a device was found, 0 if no device.
*/

uint8_t onewire0_search(void)
{
	uint8_t i;
	uint8_t id_bit_number = 1;

	for (i = 0; i < 8; ++i) {
		search0.device_id[i] = 0;
	}

	if (!onewire0_reset() || search0.last_device_flag) {
		_resetsearch();
		return 0;
	}

	search0.last_zero = 0;

	onewire0_writebyte(0xf0);

	while (id_bit_number <= 64) {
		uint8_t search_direction;

		_read2();
		_wait();

		// Pick top 2 bits of this byte only
		// bit 7 = cmp_id_bit
		// bit 6 = id_bit
		i = onewire0.current_byte & 0xc0;

		// if id_bit == cmp_id_bit == 1
		if (i == 0xc0) {
			// No device found
			_resetsearch();
			return 0;
		}

		if (i == 0x00) {
			if (id_bit_number == search0.last_discrepancy) {
				search_direction = 1;
			}
			else if (id_bit_number > search0.last_discrepancy) {
				search_direction = 0;
			}
			else {
				// Set search_direction bit to id_bit_number bit in ROM_NO
				search_direction = _getbit(search0.device_id, id_bit_number);
			}

			if (search_direction == 0) {
				search0.last_zero = id_bit_number;
				if (search0.last_zero < 9) {
					search0.last_family_discrepancy = search0.last_zero;
				}
			}
		} else {
			search_direction = (i & 0x40) ? 1 : 0;
		}

		_setbit(search0.device_id, id_bit_number, search_direction);
		_writebit(search_direction);

		id_bit_number ++;
	}

	search0.last_discrepancy = search0.last_zero;

	if (search0.last_discrepancy == 0) {
		search0.last_device_flag = 1;
	}

	if (! _checkCRC(search0.device_id)) {
		_resetsearch();
		return 0;
	}

	return 1;
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
	if (--onewire0.bit_id) {
		// Continue reading/writing a byte with the next bit
		onewire0.state = OW0_START;
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
			OCR0A = IDLE_DELAY - 1;
			break;

		case OW0_START:
			_pulllow();

			if (onewire0.current_byte & 1) {
				uint8_t delay;

				// Write a 1-bit or read a bit:
				// 6us low, 9us wait, sample, 55us high
				OCR0A = 70 - 1;

				// Delay 15 us within the interupt function:
				// 6 us signal low (48 instruction times)
				// This loop takes 5 x N instruction times
				for (delay = 0; delay < 8; ++delay) {
					asm("nop");
				}

				// 9 us tri-state
				_release();
				for (delay = 0; delay < 14; ++delay) {
					asm("nop");
				}

				// shift byte then sample the signal
				onewire0.current_byte = (onewire0.current_byte >> 1) | ((PINB & (PIN)) ? 0x80 : 0);
				// onewire0.current_byte >>= 1;
				// onewire0.current_byte |= ((PINB & (PIN)) ? 0x80 : 0);
				_nextbit();
			} else {
				// Write a 0-bit
				// 60us low, 10us high
				OCR0A = GAP_C - 1;
				onewire0.state = OW0_RELEASE;
				onewire0.current_byte >>= 1;
			}

			break;

		case OW0_READWAIT:
			// Let the signal go high, wait 9us then sample.
			_release();
			OCR0A = GAP_E - 1;
			onewire0.state = OW0_SAMPLE;
			break;

		case OW0_SAMPLE:
			// Bits are read from 0 to 7, which means we
			// have to shift current_byte down and store in bit 7
			// Shifting is done in state OW0_START so no need to do it again here.
			onewire0.current_byte |= ((PINB & (PIN)) ? 0x80 : 0);
			OCR0A = GAP_F - 1;
			_nextbit();
			break;

		case OW0_RELEASE:
			// Let the signal go high for 10us.
			_release();
			OCR0A = GAP_D + 5 - 1;
			_nextbit();
			break;

		case OW0_RESET:
			// Pull the bus down and wait 480us (slow down the prescaler)
			_pulllow();
			onewire0.ocr0a = GAP_H - 1;
			_medtimer();
			onewire0.state = OW0_RESET1;
			break;

		case OW0_RESET1:
			// Release the bus, speed up the prescaler and wait for 9us
			_release();
			OCR0A = GAP_I - 1;
			onewire0.state = OW0_RESET2;
			break;

		case OW0_RESET2:
			// Sample the bus, slow the prescaler down again and wait 408us
			onewire0.current_byte = ((PINB & (PIN)) ? 0x80 : 0);
			onewire0.ocr0a = GAP_J - 1;
			_medtimer();
			onewire0.state = OW0_RESET3;
			break;

		case OW0_RESET3:
			// Speed up the prescaler again, go to idle state with 20us between interrupts
			OCR0A = IDLE_DELAY - 1;
			_fasttimer();
			onewire0.state = OW0_IDLE;
			break;

		case OW0_DELAY1US:
			OCR0A = onewire0.ocr0a;
			// The timer is assumed to already be in fast mode
			onewire0.state = OW0_DELAY;
			break;

		case OW0_DELAY8US:
			// Setup timer to interrupt every 8 us x (ocr0a + 1), then enter delay loop
			_medtimer();
			onewire0.state = OW0_DELAY;
			break;

		case OW0_DELAY128US:
			// Setup timer to interrupt every 128 us x (ocr0a + 1), then enter delay loop
			_delaytimer();
			onewire0.state = OW0_DELAY;
			break;

		case OW0_DELAY:
			if (! --onewire0.delay_count) {
				// Delay is finished; setup the next interrupt in 20 us
				OCR0A = IDLE_DELAY - 1;
				_fasttimer();
				onewire0.state = OW0_IDLE;
			}
			break;

		case OW0_DELAY_END:
			onewire0.state = OW0_IDLE;
			break;

		case OW0_CONVERT:
			// Program a 750 ms delay
			// 750ms = 1 us * 240 * 3125
			OCR0A = 239;
			onewire0.delay_count = 3125;
			_pullhigh();
			onewire0.state = OW0_CONVERT_DELAY;
			break;

		case OW0_CONVERT_DELAY:
			if (! --onewire0.delay_count) {
				// Delay is finished; setup the next interrupt in 20 us
				OCR0A = IDLE_DELAY - 1;
				_release();
				onewire0.state = OW0_IDLE;
			}
			break;
	}


	// Return from interrupt
}

uint8_t onewire0_isidle(void) {
	return (onewire0.state == OW0_IDLE);
}

/* Return current device state. This is used for debugging.
*/

uint8_t onewire0_state(void) {
	return onewire0.state;
}

/*
**  High Level Functions
*/

void onewire0_readrom(struct onewire_id *buf) {
	uint8_t  *cp = buf->device_id;
	uint8_t  byte_id;

	onewire0_writebyte(0x33);

	for (byte_id = 0; byte_id < 8; ++byte_id) {
		*cp++ = onewire0_readbyte();
	}
}

void onewire0_matchrom(struct onewire_id *dev) {
	uint8_t  *buf = dev->device_id;
	uint8_t  byte_id;

	onewire0_writebyte(0x55);

	for (byte_id = 0; byte_id < 8; ++byte_id) {
		onewire0_writebyte(*buf++);
	}
}

void onewire0_skiprom(void) {
	onewire0_writebyte(0xcc);
}

void onewire0_readscratchpad(void) {
	onewire0_writebyte(0xbe);
}

// Issue 0x44, "Convert T".

void onewire0_convert(void) {
	onewire0_writebyte(0x44);
}

void    onewire0_convertdelay(void) {
	while (onewire0.state != OW0_IDLE) { }
	// Start a 750 ms delay, with a strong pullup to power the chips
	onewire0.state = OW0_CONVERT;
}

void onewire0_writescratch(char *scratch) {
	uint8_t  byte_id;

	onewire0_writebyte(0x4e);

	for (byte_id = 0; byte_id < 3; ++byte_id) {
		onewire0_writebyte(*scratch++);
	}
}

uint8_t onewire0_readpower(void) {
	onewire0_writebyte(0xb4);

	return _readbit();
}

uint8_t onewire0_get_family_code(struct onewire_id *dev) {
	return dev->device_id[0];
}

/*  Check an array of bytes using the Maxim 8-bit CRC algorithm.
**  The last byte must be the expected CRC to make the final
**  result zero.
*/

uint8_t onewire0_check_crc(uint8_t *cp, uint8_t length) {
	uint8_t crc = 0x00;

	while (length--) {
		crc = crc8_update(crc, *cp++);
	}

	return crc;
}
