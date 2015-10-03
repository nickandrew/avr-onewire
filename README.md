# AVR ATTiny85 OneWire protocol library

This library implements the Maxim 1-Wire protocol (see https://www.maximintegrated.com/en.html) on the AVR ATTiny85 microcontroller.

As a software implementation of 1-Wire, it is unique, or at least novel, in that the tight timing requirements are implemented through the onboard timer device. It is a very accurate implementation of 1-Wire.

This package is the work of Nick Andrew <nick@nick-andrew.net> and is not associated with Maxim Integrated nor Atmel. All code is licensed under the GNU General Public License, version 3.

## Usage

Two GPIO pins are in use, one for the 1-Wire protocol and the other one to enable a strong pullup (to provide power for passive powered devices). These pins are set at compile time.

The 1-Wire pin defaults to PORTB4 and the strong pullup pin defaults to PORTB1.

Only an 8 MHz CPU frequency is supported at present. Overdrive is not supported as the CPU is not fast enough.

See `test-harness.c` for typical usage.

The 1-Wire protocol is documented in Maxim Integrated Application Notes, including:
  * AN1796 "Overview of 1-Wire Technology and Its Use"
  * AN126 "1-Wire Communication Through Software"
  * AN187 "1-Wire Search Algorithm"
  * AN162 "Interfacing the DS18X20/DS1822 1-Wire® Temperature Sensor in a Microcontroller Environment"
  * Datasheets for the DS18B20 and DS18S20 digital thermometers
    * See https://www.maximintegrated.com/en/products/analog/sensors-and-sensor-interface/DS18B20.html

## Troubleshooting

Compile test-harness.c and use a logic analyser to examine the output at all state transitions.
