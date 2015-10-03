# @(#) Makefile for libonewire0.a

MCU = attiny85

# Optimization level, can be [0, 1, 2, 3, s]. 0 turns off optimization.
# (Note: 3 is not always the best optimization level. See avr-libc FAQ.)
OPT = s

# Optional compiler flags.
#  -g:        generate debugging information (for GDB, or for COFF conversion)
#  -O*:       optimization level
#  -f...:     tuning, see gcc manual and avr-libc documentation
#  -Wall...:  warning level
#  -Wa,...:   tell GCC to pass this to the assembler.
#    -ahlms:  create assembler listing
CFLAGS = -g -O$(OPT) \
         -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
         -Wall -Wstrict-prototypes \
         -Wa,-adhlns=$(<:.c=.lst) \
         $(patsubst %,-I%,$(EXTRAINCDIRS))

# Set a "language standard" compiler flag.
#   Unremark just one line below to set the language standard to use.
#   gnu99 = C99 + GNU extensions. See GCC manual for more information.
CFLAGS += -std=gnu99

# Combine all necessary flags and optional flags.
# Add target processor to flags.
ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS)

CC = avr-gcc

# Compile: create object files from C source files.
%.o : %.c
	@echo
	@echo Compiling $<
	$(CC) -c $(ALL_CFLAGS) $< -o $@

%.a:
	ar cru $@ $^
	ranlib $@

# ---------------------------------------------------------------------------

all:             libonewire0.a

clean:
	rm -f *.o libonewire0.a

libonewire0.a:   onewire0.o maxim-crc8.o

onewire0.o:      onewire0.c onewire0.h maxim-crc8.h
maxim-crc8.o:    maxim-crc8.c
test-harness.o:  test-harness.c onewire0.h
test-delays.o:   test-harness.c onewire0.h
