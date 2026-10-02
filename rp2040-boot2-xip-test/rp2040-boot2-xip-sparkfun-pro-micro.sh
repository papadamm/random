#!/bin/sh
# SPDX-License-Identifier: MIT
#
# rp2040-boot2-xip-sparkfun-pro-micro.sh
#
# test code to enable flash XIP for the RP2040 on sparkfun pro micro
#
# Copyright (C) 2026 Magnus Damm
#
# the LED code is taken from teeensy for sparkfun pro micro:
# https://github.com/papadamm/teeensy/commits/main/teeensy-sparkfun-pro-micro-rp2040.sh
#
# vector table snippet after boot2 loader is taken from cortex-m4
#
# the actual XIP setup code is nicked from:
# https://github.com/raspberrypi/pico-sdk/tree/master/src/rp2040/boot_stage2
#
# Copyright (c) 2019-2021 Raspberry Pi (Trading) Ltd.
# SPDX-License-Identifier: BSD-3-Clause
#
# this script builds some bundled tiny example code to generate
# a binary which may be used to test execution on the target
# see further down in the file for some ARM assembly source code
#
# set CROSS_COMPILE to point out the toolchain
# gcc-arm-none-eabi-6-2017-q2-update is known to work

# makes use of a rp2040 (with a Cortex-M0+)
BINUTILS_OPTS="-march=armv6s-m"
CLANG_OPTS="--target=arm -mcpu=cortex-m0plus"

# Probe for required software components
for e in bc cat cut grep head mktemp od rev rm tr uuencode wc which xxd \
	    ${CROSS_COMPILE}as ${CROSS_COMPILE}gcc \
	    ${CROSS_COMPILE}ld ${CROSS_COMPILE}objcopy
do
    if [ -z `which $e` ]; then
        echo "unable to detect required software component $e, exiting" >&2
        exit 1;
    fi
done

# Check that CROSS_COMPILE actually points to an compiler for ARM
AS_OPTS=${BINUTILS_OPTS}
if [ `${CROSS_COMPILE}gcc -dumpmachine | grep arm | wc -l` -ne 1 ]; then
  echo "Failed to detect ARM support in CROSS_COMPILE, exiting" >&2
  exit 1;
fi


cleanup() {
    rm -f "${t0}" "${t1}"  "${t2}" 2>/dev/null
}

trap cleanup EXIT
t0=$(mktemp)
t1=$(mktemp)
t2=$(mktemp)
for e in "${t0}" "${t1}" "${t2}"
do
    if [ -z "${e}" ]; then
        echo "Failed to create temporary file, exiting" >&2
        exit 1;
    fi
done

# High-compatibility decimal byte-stream processor (thanks to gemini)
_crc32 ()
{
  file="$1"
  crc=4294967295
  poly=3988292384

  # Use od to get decimal bytes, then loop through them
  for byte in $(od -An -v -tu1 "$file"); do
    # Calculate next CRC state using bc
    crc=$(echo "
    define xor(a, b) {
    auto res, i, p; res=0; p=1
    for (i=0; i<32; i++) {
    if ((a%2) != (b%2)) res += p
    a /= 2; b /= 2; p *= 2
    }
    return res
    }
    c = xor($crc, $byte)
    for (i=0; i<8; i++) {
    if (c % 2 == 1) c = xor(c / 2, $poly) else c = c / 2
    }
    print c
    " | bc)
  done

  # Final XOR and Hex output
  echo "obase=16; define xor(a, b) {
  auto res, i, p; res=0; p=1
  for (i=0; i<32; i++) {
  if ((a%2) != (b%2)) res += p
  a /= 2; b /= 2; p *= 2
  }
  return res
  }
  xor($crc, 4294967295)" | bc
}

bitrev ()
{
  xxd -b -c1 | cut -f 2 -d " " | rev | \
    while read rev_byte
    do
      echo "0: ${rev_byte}" | xxd -r -b
    done
}

invert ()
{
  xxd -b -c1 | cut -f 2 -d " " | \
    while read byte
    do
      inv_byte=`echo ${byte} | tr 0 x | tr 1 0 | tr x 1`
      echo "0: ${inv_byte}" | xxd -r -b
    done
}

eight_chars ()
{
   echo 00000000${1} | rev | head -c 8 | rev
}

# turn on break-on-failure
set -e

emit_asm () {
  cat <<EOF
  .syntax unified
  .cpu cortex-m0plus
  .text

  .global boot2_start
  .type boot2_start, %function
boot2_start:
  /* this code is based on the following: */
  /* hardware_regs/include/hardware/regs/addressmap.h */
  /* hardware_regs/include/hardware/regs/ssi.h */
  /* pico_platform/include/pico/asm_helper.S */
  /* the main logic is in boot2_generic_03h.S */
  /* asminclude/boot2_helpers/exit_from_boot2.S */

#define XIP_BASE 0x10000000
#define XIP_SSI_BASE 0x18000000

#define CTRLR0_XIP 0x001f0300
#define SPI_CTRLR0_XIP 0x03000218
#define PICO_FLASH_SPI_CLKDIV 4

#define SSI_SSIENR_OFFSET 0x00000008
#define SSI_BAUDR_OFFSET 0x00000014
#define SSI_CTRLR0_OFFSET 0x00000000
#define SSI_CTRLR1_OFFSET 0x00000004
#define SSI_SPI_CTRLR0_OFFSET 0x000000f4

#define PPB_BASE 0xe0000000
#define M0PLUS_VTOR_OFFSET 0x0000ed08

  push {lr}

  ldr r3, =XIP_SSI_BASE
  movs r1, #0
  str r1, [r3, #SSI_SSIENR_OFFSET]
  movs r1, #PICO_FLASH_SPI_CLKDIV
  str r1, [r3, #SSI_BAUDR_OFFSET]
  ldr r1, =(CTRLR0_XIP)
  str r1, [r3, #SSI_CTRLR0_OFFSET]
  ldr r1, =(SPI_CTRLR0_XIP)
  ldr r0, =(XIP_SSI_BASE + SSI_SPI_CTRLR0_OFFSET)
  str r1, [r0]
  movs r1, #0x0
  str r1, [r3, #SSI_CTRLR1_OFFSET]
  movs r1, #1
  str r1, [r3, #SSI_SSIENR_OFFSET]

  pop {r0}
  cmp r0, #0
  beq vector_into_flash
  bx r0
vector_into_flash:
  ldr r0, =(XIP_BASE + 0x100)
  ldr r1, =(PPB_BASE + M0PLUS_VTOR_OFFSET)
  str r0, [r1]
  ldmia r0, {r0, r1}
  msr msp, r0
  bx r1

  .align 2
  .pool
EOF
}

emit_ldscript2 ()
{
cat <<EOF
SEARCH_DIR(.)

MEMORY
{
  FLASH (rx) : ORIGIN = $1, LENGTH = $2
}

ENTRY(vector_table)

SECTIONS
{
    .text :
    {
        *(.text*)
        *(.rodata*)
    } > FLASH

    . = ALIGN(4);
    __etext = .;
}
EOF
}

emit_asm2 () {
  cat <<EOF
  .syntax unified
  .text
  .global vector_table
vector_table:
  .long 0 /* Top of stack set to nothing since unused */
  .long _start
  .space 126 * 4

  .align 1
  .global _start
  .type _start, %function
_start:
  /* initialize GPIO25 as output for WS2812-2020 control */

  ldr r0, =0x00000120 /* PADS_BANK0 + IO_BANK0 */
  ldr r5, =(0x4000c000 + 0x3000) /* RESET_RESETS (CLR alias) */
  str r0, [r5]

  movs r0, #5
  ldr r5, =(0x400140cc + 0x0000) /* GPIO25_CTRL (normal alias) */
  str r0, [r5]

  ldr r0, =(1 << 25) /* GPIO25 */
  ldr r5, =(0xd0000024 + 0x0000) /* GPIO_OE_SET (normal alias) */
  str r0, [r5]

  /* generate a waveform for WS2812-2020 */
  /* 1 x 300us reset */
  /* 24 x ( T1H (740ns) + T1L (370ns) ) */

  ldr r5, =(0xd0000014 + 0x0000) /* GPIO_OUT_SET (normal alias) */
  ldr r6, =(0xd0000018 + 0x0000) /* GPIO_OUT_CLR (normal alias) */

  movs r3, #1  /* first run one dummy change */
  movs r4, #2  /* reset + second run when cached */

  str r0, [r5] /* low to high (initial setup, end of T0H, start of T1H) */
loop:
  str r0, [r6] /* clear or toggle */
  nop
loop2:
  str r0, [r6] /* clear or toggle */
  subs r3, #1
  bne loop

  ldr r2, =410  /* duration of a 300 us reset pulse */ 
dly:
  subs r2, #1
  bne dly

  ldr r6, =(0xd000001c + 0x0000) /* GPIO_OUT_XOR (normal alias) */
  movs r3, #25 /* generate 24 identical pulses */
  subs r4, #1
  bne loop2

  /* force the data pin low as final step */
  ldr r6, =(0xd0000018 + 0x0000) /* GPIO_OUT_CLR (normal alias) */
  str r0, [r6] /* this turns on U3 WS2812-2020 */

end:
  b end

  .align 2
  .pool
EOF
}

# generate a binary from the source, store padded result in FIRST_252
emit_asm | ${CROSS_COMPILE}gcc ${AS_OPTS} -E - | ${CROSS_COMPILE}as ${AS_OPTS} -mlittle-endian -o "${t1}"
${CROSS_COMPILE}objcopy "${t1}" -O binary "${t0}"
dd if="${t0}" bs=252 count=1 conv=sync of="${t1}" 2> /dev/null
FIRST_252=`cat "${t1}" | xxd -ps`

# checksum needs to be present for the bootrom to accept the code
cat "${t1}" | bitrev > "${t0}" # bitrev the payload before checksumming
CRCN=`_crc32 "${t0}"` # calculate a regular CRC32
CRC8=`eight_chars "${CRCN}"` # force 8 characters to work with leading zeroes
CRC=`echo "${CRC8}" | xxd -r -ps | bitrev | invert | xxd -ps` # rev, inv

# code is in little endian, store checksum in big endian
( echo ".long 0x${CRC}"; ) \
 | ${CROSS_COMPILE}as ${AS_OPTS} -mbig-endian -o "${t0}"
${CROSS_COMPILE}objcopy "${t0}" -O binary "${t1}"


# uuencode padded code followed by checksum to stdout (used as file.uue below)
# for skip-led-code encode as base64 with "uuencode -m"
  if [ "$1" == "skip-led-code" ]; then
( echo "${FIRST_252}" | xxd -r -ps; cat "${t1}"; ) | uuencode -m -
  else
( echo "${FIRST_252}" | xxd -r -ps; cat "${t1}";
  # build the code present after boot2 with linker script
  emit_ldscript2 0x10000100 0x300 > "${t2}"
  emit_asm2 | ${CROSS_COMPILE}gcc ${AS_OPTS} -E - | ${CROSS_COMPILE}as ${AS_OPTS} -mlittle-endian -o "${t1}"
  ${CROSS_COMPILE}ld "-T${t2}" "${t1}" -o "${t0}"
  ${CROSS_COMPILE}objcopy "${t0}" -O binary "${t1}"; cat "${t1}";
) | uuencode -
fi

# convert, inspect, disassemble, program
#
# cat file.uue | uudecode -o /dev/stdout > file.bin
# hexdump -C file.bin
# arm-none-eabi-objdump -b binary -m arm -M force-thumb -D file.bin
# picotool load -t bin file.bin
