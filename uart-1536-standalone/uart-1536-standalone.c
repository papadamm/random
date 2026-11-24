/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* uart-1536-standalone.c - example code to use 1536 with an UART            */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* Requires the following software components [known working revision]:      */
/* - Hayley (https://github.com/papadamm/hayley) [commit 5a9d690]            */
/* - 1536 (https://github.com/papadamm/random/) [commit 57b3fa9]             */
/*                                                                           */
/* List of target platforms and their known working toolchains:              */
/* - ch32v003f4p6-r0-1v1 (CH32V003 with FTDI UART-to-USB module)             */
/*   riscv-none-elf-gcc (GCC 12.2.0 and Binutils 2.38)                       */
/* - Seeeduino Nano (Arduino Nano clone with FTDI chipset using atmega328p)  */
/*   GCC (avr-gcc 7.3.0) and Binutils (2.26.20160125)                        */
/*                                                                           */
/* To test, connect via a terminal program to the serial port using 9600n81. */

#include <stdint.h>
void *memset(void *b, int c, int len);
void *memcpy(void *dst, void *src, int n);
#define memmove memcpy /* hack warning */

#define __weak __attribute__((__weak__))
__weak void uart_setup(void) {}
__weak void uart_tx(char ch) {}
__weak int uart_rx_available(void) { return 0; }
__weak char uart_rx(void) { return 'X'; } /* yet another hack warning */

static int __read_char(uint8_t *buf)
{
  char ch;

  while (1) {
    while (!(uart_rx_available())) {
      __asm ("nop");
    }

    ch = uart_rx();

    if (ch == '\r') {
      uart_tx('\r');
      uart_tx('\n');
    } else {
      *buf = ch;
      return 1;
    }
  }
}

#define __STANDALONE__
//#define __BUILD_ENCODER__
#define __BUILD_DECODER__
#define __INTERNAL_DIV_REM_53__
#define __INTERNAL_MUL_53__
//#define __INTERNAL_DIV_REM_10__
#define NULL (void *)0

static void div_rem_10(uint16_t dividend,
                       uint16_t *quotient, uint16_t *remainder)
{
  uint16_t res;
  uint16_t left = dividend;
  uint16_t divisor = 10;

  res = 0;
  while (left >= divisor) {
    res++;
    left -= divisor;
  }

  *quotient = res;
  *remainder = left;
}

#include "../1536/1536.c"

static int read_1536(struct context_1536 *ctx, uint8_t *buf, int bytes)
{
  int n;

  for (n = 0; n < bytes; n++) {
    __read_char(&buf[n]);
  }
  return n;
}

static int write_1536(struct context_1536 *ctx, uint8_t *buf, int bytes)
{
  int n;

  for (n = 0; n < bytes; n++) {
    uart_tx(buf[n]);
  }
  return n;
}

static void flush_1536(struct context_1536 *ctx) {}
static void err_1536(struct context_1536 *ctx, err_t err) {}

int main(void)
{
  struct context_1536 ctx = {};

  ctx.err = err_1536;
  ctx.read = read_1536;
  ctx.write = write_1536;
  ctx.flush = flush_1536;

  uart_setup();

  while (1) {
#ifdef __BUILD_DECODER__
    read_data(&ctx, OUTPUT_UNIT_BUFSIZE, decode_frame);
#endif
#ifdef __BUILD_ENCODER__
    read_data(&ctx, INPUT_BUFSIZE, encode_frame);
#endif
  }

  return 0;
}
