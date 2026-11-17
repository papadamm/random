#ifdef SEEEDUINO_NANO /* UART code for Seeeduino Nano */
  .global uart_setup
  .type uart_setup, %function
uart_setup:
  /* Turn off UART sleep mode */
  lds r18, 0x0064 /* PRR */
  cbr r18, (1 << 1) /* bit 1: PRUSART0 */
  sts 0x0064, r18 /* PRR */

  /* Set baud rate */
  ldi r18, 0x00
  sts 0x00c5, r18 /* UBRRnH */
  ldi r18, 103 /* 9600 bps with 16 MHz XTAL */
  sts 0x00c4, r18 /* UBRRnL */

  /* Enable receiver and transmitter */
  ldi r18, 0x18 /* (1<<RXENn)|(1<<TXENn) */
  sts 0x00c1, r18 /* UCSRnB */

  /* Set frame format: 8data, 1stop bit */
  ldi r18, 0x06 /* (3<<UCSZn0) */
  sts 0x00c2 ,r18 /* UCSRnC */
  ret

  .global uart_tx
  .type uart_tx, %function
uart_tx:
  /* Wait for empty transmit buffer */
  lds r18, 0x00c0 /* UCSRnA */
  sbrs r18, 5 /* bit 5: UDREn */
  rjmp uart_tx

  /* Put data into buffer, sends the data */
  sts 0x00c6, r24 /* UDRn */
  ret

  .global uart_rx
  .type uart_rx, %function
uart_rx:
  clr r24

  /* Read out if unread data is available */
  lds r18, 0x00c0 /* UCSRnA */
  sbrc r18, 7 /* bit 7: RXCn */
  lds r24, 0x00c6 /* UDRn */
  ret

  .global uart_rx_available
  .type uart_rx_available, %function
uart_rx_available:
  clr r24
  clr r25 /* Return 16-bit integer */

  /* Set return value if unread data is available */
  lds r18, 0x00c0 /* UCSRnA */
  sbrc r18, 7 /* bit 7: RXCn */
  ori r24, 1
  ret
#endif
