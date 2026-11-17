#ifdef CH32V003F4P6_R0_1V1 /* USART code for CH32V003F4P6_R0_1V1 @ PD5, PD6 */
  .global uart_setup
  .type uart_setup, %function
uart_setup:
  /* assumes TX is at PD5, RX is at PD6 (reset default) */

  li t0, 0x00004020 /* USART1EN=1, IOPDEN=1 */
  li t1, 0x40021018 /* RCC_APB2PCENR */
  sw t0, 0(t1)

  li t0, 0x44944444 /* MODE5[1:0], CNF5[1:0] PD5 setup, others reset default */
  li t1, 0x40011400 /* R32_GPIOD_CFGLR */
  sw t0, 0(t1)

  li t0, 0x00000340 /* ~9600 bps at reset default */
  li t1, 0x40013808 /* R32_USART_BRR */
  sw t0, 0(t1)

  li t0, 0x0000200c /* UE=1, TE=1, RE=1 */
  li t1, 0x4001380c /* R32_USART_CTLR1 */
  sw t0, 0(t1)
  ret

  .global uart_tx
  .type uart_tx, %function
uart_tx:

  li t1, 0x40013800 /* R32_USART_STATR */
  lw t2, 0(t1)
  andi t0, t2, 1 << 7 /* TXE bit 7: 1 = data register empty */
  beqz t0, uart_tx

  li t1, 0x40013804 /* R32_USART_DATAR */
  sw a0, 0(t1)
  ret

  .global uart_rx
  .type uart_rx, %function
uart_rx:

  li t1, 0x40013800 /* R32_USART_STATR */
  lw t2, 0(t1)
  andi a0, t2, 1 << 5 /* RXNE bit 5: 1 = data available */
  beqz a0, uart_rx

  li t1, 0x40013804 /* R32_USART_DATAR */
  lw a0, 0(t1)
  ret

  .global uart_rx_available
  .type uart_rx_available, %function
uart_rx_available:

  li t1, 0x40013800 /* R32_USART_STATR */
  lw t2, 0(t1)
  andi a0, t2, 1 << 5 /* RXNE bit 5: 1 = data available */
  ret
#endif
