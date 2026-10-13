/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* uart-1654-standalone.c - example code to use 1654 with an UART            */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* Requires the following software components [known working revision]:      */
/* - Hayley (https://github.com/papadamm/hayley) [commit c4c16cd]            */
/* - 1654 (https://github.com/papadamm/random/) [commit e175aed]             */
/*                                                                           */
/* List of target platforms and their known working toolchains:              */
/* - Seeeduino Nano (Arduino Nano clone with FTDI chipset using atmega328p)  */
/*   GCC (avr-gcc 7.3.0) and Binutils (2.26.20160125)                        */
/*                                                                           */
/* To test, connect via a terminal program to the serial port using 9600n81. */
/* The decoder may be tested by transmitting the characters R]LR}v that will */
/* be decoded on the target system and results in the character string CODE. */
/*                                                                           */
/* For more information see tutorials included in the 1654 README.md file    */

#include <stdint.h>

#define __weak __attribute__((__weak__))
__weak void uart_setup(void) {}
__weak void uart_tx(char ch) {}
__weak int uart_rx_available(void) { return 0; }
__weak char uart_rx(void) { return 'X'; }

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
#define NULL (void *)0
static void err(const char *err_msg) { /* no error reporting */ }
int read_bin(uint8_t *buf) { return __read_char(buf); }
int write_char(uint8_t data) { uart_tx(data); return 1; }
int read_char(uint8_t *buf) { return __read_char(buf); }
int write_bin(uint8_t data) { uart_tx(data); return 1; }
#include "../1654/1654.c"

int main(void)
{
  uart_setup();

  while (1) {
#ifdef __BUILD_DECODER__
    decode_54(1, 1, NULL, NULL);
#endif
#ifdef __BUILD_ENCODER__
    encode_54d(1);
#endif
  }

  return 0;
}
