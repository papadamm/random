#!/bin/sh
# SPDX-License-Identifier: MIT
#
# this hack for ch32v003f4p6-r0-1v1 will play 0-9 DTMF tones on PD0
#
# Copyright (C) 2026 Magnus Damm
#
# a sparkfun RedBot Buzzer or equivalent should be connected to PD0
# also a logic analyzer like a Saleae Logic is helpful to determine
# the sample frequency for the 1-bit Delta-Sigma software DAC.
# right now one sample takes ~650 ns @ 48 MHz configuration which
# results in a 1.5 MHz sample rate, see the output_asm() param. please
# note that needs to be updated if the assembly code timing is changed
#
# set CROSS_COMPILE to point out the toolchain
# riscv32-unknown-elf from binutils-2.47 is known to work

# ch32v003f4p6-r0-1v1 (ch32v003 with a QingKe V2A core (RISC-V RV32EC))
BINUTILS_OPTS="-march=rv32ec"

# Probe for required software components
for e in bc cat grep mktemp rm wc which ${CROSS_COMPILE}as \
	     ${CROSS_COMPILE}ld ${CROSS_COMPILE}objcopy
do
    if [ -z `which $e` ]; then
        echo "unable to detect required software component $e, exiting" >&2
        exit 1;
    fi
done

# Check that CROSS_COMPILE actually points to an assembler for RISC-V
AS_OPTS=${BINUTILS_OPTS}
${CROSS_COMPILE}as ${BINUTILS_OPTS} /dev/null -o /dev/null 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Failed to detect RISC-V support in CROSS_COMPILE, exiting" >&2
    exit 1;
fi

cleanup() {
    rm -f "${t0}" "${t1}" 2>/dev/null
}

trap cleanup EXIT
t0=$(mktemp)
t1=$(mktemp)
for e in "${t0}" "${t1}"
do
    if [ -z "${e}" ]; then
        echo "Failed to create temporary file, exiting" >&2
        exit 1;
    fi
done

# turn on break-on-failure
set -e

output_asm ()
{
cat <<EOF
  .text
  .global _start
  .type _start, %function
_start:
  /* turn on clock to GPIO PORTD via IOPDEN bit in RCC_APB2PCENR */

  li t0, 0x20 /* IOPDEN=1 */
  li t1, 0x40021018 /* RCC_APB2PCENR */
  sw t0, 0(t1)

  /* enable PLL with HSI to double operating frequency */

  li t0, 1 << 24 /* PLLON */
  li t1, 0x40021000 /* R32_RCC_CTLR */
  sw t0, 0(t1)

  li t0, 2 /* SW[1:0] = 0b10 (PLL) */
  li t1, 0x40021004 /* R32_RCC_CFGR0 */
  sw t0, 0(t1)

  /* configure PD0 pin */

  li t0, 0x44444443 /* MODE0[1:0], CNF0[1:0] setup, others reset default */
  li t1, 0x40011400 /* R32_GPIOD_CFGLR */
  sw t0, 0(t1)

  /* a2: the GPIO value used to set the pin */
  /* a3: the GPIO value used to clear the pin */
  /* a4: the GPIO clear/set register */

  li a2, 1 << 0 /* set GPIO output to 1 */
  li a3, 1 << 16 /* clear GPIO output to 0 */
  li a4, 0x40011410 /* R32_GPIOD_BSHR */

  /* a5: shared sample accumulator for Delta-Sigma DAC */
  /* gp: index into playback string */

  li a5, 0

start_over:

  li gp, 0

next_tone:
  /* ra: sample down counter, used to keep track of the duration of one tone */
  li ra, $1 / 3

  /* decode ascii string and determine which tones to play */
  la t0, string
  add t1, gp, t0
  lb t0, 0(t1)
  li t1, 0
  beq t0, t1, hang

  li t1, ' '
  beq t0, t1, space
  addi t2, t0, -'0'

  la t0, digit2bandX
  add t0, t0, t2
  lb t0, 0(t0)
  slli t0, t0, 2

  la t1, digit2bandY
  add t1, t1, t2
  lb t1, 0(t1)
  slli t1, t1, 2

  /* a0: tone X playback pitch */
  /* a1: tone Y playback pitch */
  /* s0: tone X playback counter */
  /* s1: tone Y playback counter */

  la a0, freqX
  add a0, a0, t0
  lw a0, 0(a0)

  la a1, freqY
  add a1, a1, t1
  lw a1, 0(a1)

  j start_zero

space:
  li a0, 0
  li a1, 0

start_zero:
  la s0, 0
  la s1, 0

next_sample:
  /* retrieve X, temporarily store in t2 */
  srli t0, s0, 8
  andi t0, t0, 255
  la t1, sine
  add t2, t1, t0
  lb t2, 0(t2)

  /* retrieve Y, temporarily store in t1 */
  srli t0, s1, 8
  andi t0, t0, 255
  la t1, sine
  add t1, t1, t0
  lb t1, 0(t1)

  add a5, a5, t1
  add a5, a5, t2
  bgez a5, acc_pos

  addi a5, a5, 256
  sw a3, 0(a4) /* clear GPIO output to 0 */
  j end_sample

acc_pos:
  addi a5, a5, -256
  sw a2, 0(a4) /* set GPIO output to 1 */
  nop
  
end_sample:
  add  s0, s0, a0
  add  s1, s1, a1

  addi ra, ra, -1
  bnez ra, next_sample

  addi gp, gp, 1
  j next_tone

hang:
  j hang

string:
  .ascii "0 1 2 3 4 5 6 7 8 9"

 /* ['0'], ['1'] .. ['9'] */
digit2bandX:
  .byte 3, 0, 0, 0, 1, 1, 1, 2, 2, 2

digit2bandY:
  .byte 1, 0, 1, 2, 0, 1, 2, 0, 1, 2

  .macro  snd freq
  .long   (256 * 256) / ($1 / \freq)
  .endm

  .align 2

freqX:
  snd(697)
  snd(770)
  snd(852)
  snd(941)

freqY:
  snd(1209)
  snd(1336)
  snd(1477)
  snd(1633)

sine:
EOF

# calculate the sine table and embed into the assembly source code
#
# a total of 256 byte entries for one 360 degree period of sine
# stored in 8-bit entries in the range between +127 to - 127
#
# to generate a tone, sine data is looked up from this table

for e in `seq 256`
do
    n=`echo "r(sin(d2r(($e * 360) / 256)) * 127, 0)" | bc -l`
    echo ".byte $n"
done
}

# the Delta-Sigma software DAC runs at 1.5 MHz
output_asm 1500000 | ${CROSS_COMPILE}as ${AS_OPTS} -o "${t0}"
${CROSS_COMPILE}ld --section-start=.text=0x08000000 "${t0}" -o "${t1}"
${CROSS_COMPILE}objcopy "${t1}" -O ihex "${t0}"
cat "${t0}" # the contents come out on stdout, used as "file.hex" below

# use a WCH-LinkE hardware debugger with open source wlink tool
# wlink version 0.1.2 is known to work on Mac OS X
#
# connect 3V3 and GND from hardware debugger to power the target board
# also connect SWDIO/TMS pin from hardware debugger to target board PD1
#
# % wlink flash file.hex
# % wlink erase
