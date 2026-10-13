HL :=
$(if $(shell echo $(HL)),, $(error Unable to parse HL environment variable))

CROSS_COMPILE := $(shell $(HL) get-cross-compile)
CFLAGS := $(shell $(HL) get-cflags) $(shell $(HL) get-extra-flags)
ASMFLAGS := $(shell $(HL) get-asmflags)
OBJ := uart-1654-standalone.o uart-seeeduino-nano.o

%.o: %.S
	$(CROSS_COMPILE)gcc $(CFLAGS) -E $< | $(CROSS_COMPILE)as $(ASMFLAGS) - -o $@
%.o: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -Wall -c $< -o $@

default: file.hex
clean:
	rm -f $(OBJ) file.hex

file.hex: $(OBJ)
	$(HL) link $^ > $@
