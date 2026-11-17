HL :=
$(if $(shell echo $(HL)),, $(error Unable to parse HL environment variable))

CROSS_COMPILE := $(shell $(HL) get-cross-compile)
CFLAGS := $(shell $(HL) get-cflags) $(shell $(HL) get-extra-flags)
ASMFLAGS := $(shell $(HL) get-asmflags)
LDFLAGS := $(shell $(HL) get-ldflags)
OBJ := uart-1536-standalone.o uart-seeeduino-nano.o uart-ch32v003f4p6-r0-1v1.o

%.o: %.S
	$(CROSS_COMPILE)gcc $(CFLAGS) -E $< | $(CROSS_COMPILE)as $(ASMFLAGS) - -o $@
%.o: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) -Wall -c $< -o $@

.PHONY: file.hex
default: file.hex
clean:
	rm -f $(OBJ) file.hex

file.hex: $(OBJ)
	$(HL) link $^ $(LDFLAGS) > $@
