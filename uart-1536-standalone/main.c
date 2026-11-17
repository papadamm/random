#define __weak __attribute__((__weak__))
__weak void uart_setup(void) {}
__weak void uart_tx(char ch) {}
__weak int uart_rx_available(void) { return 0; }
__weak char uart_rx(void) { return 'X'; }

#define LINE_MAXCHARS 8

char line_buf[LINE_MAXCHARS];
unsigned int line_pos = 0;

const char message[] = { 'h', 'e', 'l', 'l', 'o' };

static void print_state(void)
{
  int k;

  for (k = 0; k < sizeof(message); k++) {
    uart_tx(message[k]);
  }

  uart_tx('\r');
  uart_tx('\n');
}

int main(void)
{
  char ch;

  uart_setup();

  while (1) {
    __asm ("nop");

    if (uart_rx_available()) {
      ch = uart_rx();

      if (ch == '\b') {
        uart_tx('\b');
        uart_tx(' ');
        uart_tx('\b');
        if (line_pos > 0)
          line_pos--;
      } else if (ch == '\r') {
        if (line_pos != 0) {
          uart_tx('\r');
          uart_tx('\n');
        }
        print_state();
        line_pos = 0;
      } else if (line_pos < LINE_MAXCHARS) {
        uart_tx(ch);
        line_buf[line_pos] = ch;
        line_pos++;
      }
    }
  }

  return 0;
}
