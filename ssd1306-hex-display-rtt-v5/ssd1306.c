#include <stdint.h>

#include "gpio.h"

// try to make use of the Pixhawk 6C USB connector, this works so-so
//#define PIN_SDA GPIO_STM32H7(GPIOA_PORT, 11)
//#define PIN_SCL GPIO_STM32H7(GPIOA_PORT, 12)
// repurpose the PWM output FMU_CH5 and FMU_CH6, maybe that works better
#define PIN_SDA GPIO_STM32H7(GPIOD_PORT, 14) // CH5
#define PIN_SCL GPIO_STM32H7(GPIOD_PORT, 15) // CH6

/* I2C sequencer */

#define SEQ_SDA_L SEQ_CLEAR(0)
#define SEQ_SDA_H SEQ_SET(0)
#define SEQ_SCL_L SEQ_CLEAR(1)
#define SEQ_SCL_H SEQ_SET(1)

static void i2c_seq_start(void)
{
  /* generate start condition */
  gpio_seq_run(GPIO_SEQ_PINS( PIN_SDA, PIN_SCL ),
               GPIO_SEQ_OPCODES(
                                 SEQ_SET(0), SEQ_SET(1),
                                 SEQ_MODE_OUTPUT(0), SEQ_MODE_OUTPUT(1),
                                 /* generate start condition */
                                 SEQ_SDA_L, SEQ_BARRIER,
                                 SEQ_SCL_L, SEQ_BARRIER, ), 0, 0);
}

static int i2c_seq_out8(uint8_t data)
{
  gpio_sequence_data_out_t data_out;
  gpio_sequence_data_in_t data_in;
  int k;

  data_in = GPIO_SEQ_DATA_IN(0);
  data_out = GPIO_SEQ_DATA_OUT(0, 0, 0, 0, 0, 0, 0, 0);

  for (k = 0; k < 8; k++) {
    data_out.data_out[k] = data & 0x01;
    data >>= 1;
  }

  /* output 8 bits and read ack */
  gpio_seq_run(GPIO_SEQ_PINS( PIN_SDA, PIN_SCL ),
               GPIO_SEQ_OPCODES(
                                 /* clock out bit 7 */
                                 SEQ_DATA_OUT(7), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock out bit 6 */
                                 SEQ_DATA_OUT(6), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock out bit 5 */
                                 SEQ_DATA_OUT(5), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock out bit 4 */
                                 SEQ_DATA_OUT(4), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock out bit 3 */
                                 SEQ_DATA_OUT(3), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock out bit 2 */
				 SEQ_DATA_OUT(2), SEQ_SET_VALUE(0),
				 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
				 /* clock out bit 1 */
				 SEQ_DATA_OUT(1), SEQ_SET_VALUE(0),
				 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
				 /* clock out bit 0 */
				 SEQ_DATA_OUT(0), SEQ_SET_VALUE(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_SCL_L,
                                 /* ACK/NACK handling */
                                 SEQ_MODE_INPUT(0), SEQ_DATA_IN(0),
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_GET_VALUE(0),
                                 SEQ_BARRIER, SEQ_SCL_L, SEQ_BARRIER,
                                 SEQ_SDA_H, SEQ_MODE_OUTPUT(0),
                                 SEQ_BARRIER ), &data_out, &data_in);

  return data_in.data_in[0];
}

static void i2c_seq_stop(void)
{
  /* generate start condition */
  gpio_seq_run(GPIO_SEQ_PINS( PIN_SDA, PIN_SCL ),
               GPIO_SEQ_OPCODES(
                                 SEQ_SDA_L, SEQ_BARRIER,
                                 SEQ_SCL_L, SEQ_BARRIER,
                                 /* generate stop condition */
                                 SEQ_SCL_H, SEQ_BARRIER,
                                 SEQ_SDA_H, SEQ_BARRIER, ), 0, 0);
}

void __i2c_write(void *vp,
		 uint8_t device_address,
		 uint8_t *tx_buf, unsigned int tx_len)
{
  int k;
  
  i2c_seq_start();
  i2c_seq_out8(device_address << 1);

  for (k = 0; k < tx_len; k++) {
    i2c_seq_out8(tx_buf[k]);
  }
  
  i2c_seq_stop();
}

void hack_i2c_init(void)
{
  gpio_set_value(PIN_SCL, 1);
  gpio_set_value(PIN_SDA, 1);

  /* Open-Drain, Enable On-Chip Pull-Ups */
  gpio_set_mode_detail(PIN_SDA, GPIO_MODE_INPUT, /* as input use this mode */
		       GPIO_MODE_DETAIL_STM32H7(0x00, 0x00, 0x00, 0x01));
  gpio_set_mode_detail(PIN_SDA, GPIO_MODE_OUTPUT, /* as output use this mode */
		       GPIO_MODE_DETAIL_STM32H7(0x01, 0x01, 0x00, 0x01));
  gpio_set_mode_detail(PIN_SCL, GPIO_MODE_INPUT, /* as input use this mode */
		       GPIO_MODE_DETAIL_STM32H7(0x00, 0x00, 0x00, 0x01));
  gpio_set_mode_detail(PIN_SCL, GPIO_MODE_OUTPUT, /* as output use this mode */
		       GPIO_MODE_DETAIL_STM32H7(0x01, 0x01, 0x00, 0x01));

  gpio_set_direction(PIN_SCL, GPIO_DIR_OUTPUT);
  gpio_set_direction(PIN_SDA, GPIO_DIR_OUTPUT);
}

static void send_cmd1(uint8_t data)
{
  uint8_t buf[2];

  buf[0] = 0x00;
  buf[1] = data;
  
  __i2c_write(0, 0x3c, &buf[0], sizeof(buf));
}

static void send_cmd2(uint8_t data1, uint8_t data2)
{
  send_cmd1(data1);
  send_cmd1(data2);
}

static void send_cmd3(uint8_t data1, uint8_t data2, uint8_t data3)
{
  send_cmd1(data1);
  send_cmd1(data2);
  send_cmd1(data3);
}

static void ssd1306_command(uint8_t data)
{
  send_cmd1(data);
}

static void send_data(uint8_t data)
{
  uint8_t buf[2];

  buf[0] = 0x40;
  buf[1] = data;
  
  __i2c_write(0, 0x3c, &buf[0], sizeof(buf));
}

uint8_t swap_table[] = { 
  0x0, 0x8, 0x4, 0xc, 0x2, 0x9, 0x6, 0xe,
  0x1, 0x9, 0xf, 0xd, 0x3, 0xb, 0x7, 0xf
};

static void draw_swap_at(uint8_t c, uint8_t row, uint8_t column, uint8_t offset)
{
  uint8_t data;

  data = swap_table[c & 0x0f] << 4;
  data |= swap_table[c >> 4];

  send_cmd3(0x21, row + offset, 0x7f);
  send_cmd3(0x22, column, 0x07);

  send_data(data);
}

uint16_t font_data[] = {
  0x0b73, 0x0480, 0x033f, 0x093b,
  0x094c, 0x087f, 0x0a7f, 0x6003,
  0x0b7f, 0x094f, 0x0b4f, 0x0a7c,
  0x023c, 0x0b3c, 0x027f, 0x0247
};

static void draw_char(uint8_t x, uint8_t y, uint8_t c)
{
  uint8_t column = x % 8;
  uint8_t row = (y % 12) * 10;
  uint16_t f = font_data[c];
  uint8_t data;

  /* draw first row */
  data = f & (1 << 0) ? 0x60 : 0x00;
  data |= f & (1 << 1) ? 0x0c : 0x00;
  draw_swap_at(data, row, column, 0);

  /* draw second row */
  data = f & (1 << 6) ? 0x80 : 0x00;
  data |= f & (1 << 7) ? 0x10 : 0x00;
  data |= f & (1 << 8) ? 0x02 : 0x00;
  data |= f & (1 << 12) ? 0x40 : 0x00;
  data |= f & (1 << 13) ? 0x04 : 0x00;
  draw_swap_at(data, row, column, 1);

  /* draw third row */
  data = f & (1 << 6) ? 0x80 : 0x00;
  data |= f & (1 << 7) ? 0x10 : 0x00;
  data |= f & (1 << 8) ? 0x02 : 0x00;
  data |= f & (1 << 12) ? 0x60 : 0x00;
  data |= f & (1 << 13) ? 0x0c : 0x00;
  draw_swap_at(data, row, column, 2);

  /* draw fourth row */
  data = f & (1 << 6) ? 0x80 : 0x00;
  data |= f & (1 << 7) ? 0x10 : 0x00;
  data |= f & (1 << 8) ? 0x02 : 0x00;
  data |= f & (1 << 12) ? 0x20 : 0x00;
  data |= f & (1 << 13) ? 0x08 : 0x00;
  draw_swap_at(data, row, column, 3);
  
  /* draw fifth row */
  data = f & (1 << 2) ? 0x60 : 0x00;
  data |= f & (1 << 3) ? 0x0c : 0x00;
  draw_swap_at(data, row, column, 4);

  /* draw sixth row */
  data = f & (1 << 9) ? 0x80 : 0x00;
  data |= f & (1 << 10) ? 0x10 : 0x00;
  data |= f & (1 << 11) ? 0x02 : 0x00;
  data |= f & (1 << 14) ? 0x20 : 0x00;
  data |= f & (1 << 15) ? 0x08 : 0x00;
  draw_swap_at(data, row, column, 5);

  /* draw seventh row */
  data = f & (1 << 9) ? 0x80 : 0x00;
  data |= f & (1 << 10) ? 0x10 : 0x00;
  data |= f & (1 << 11) ? 0x02 : 0x00;
  data |= f & (1 << 14) ? 0x60 : 0x00;
  data |= f & (1 << 15) ? 0x0c : 0x00;
  draw_swap_at(data, row, column, 6);

  /* draw eigth row */
  data = f & (1 << 9) ? 0x80 : 0x00;
  data |= f & (1 << 10) ? 0x10 : 0x00;
  data |= f & (1 << 11) ? 0x02 : 0x00;
  data |= f & (1 << 14) ? 0x40 : 0x00;
  data |= f & (1 << 15) ? 0x02 : 0x00;
  draw_swap_at(data, row, column, 7);

  /* draw nineth row */
  data = f & (1 << 4) ? 0x60 : 0x00;
  data |= f & (1 << 5) ? 0x0c : 0x00;
  draw_swap_at(data, row, column, 8);
}

const uint8_t ssd1306_init_sequence[] = {
  0xae, 0xd5, 0x80, 0xa8,
  0x3f, 0xd3, 0x00, 0x40,
  0x8d, 0x14, 0x20, 0x00,
  0xa1, 0xc8, 0xda, 0x12,
  0x81, 0xcf, 0xd9, 0xf1,
  0xdb, 0x40, 0xa4, 0xa6,
  0x2e,
};

void test_display()
{
  int i;

  for (i = 0; i < sizeof(ssd1306_init_sequence); i++) {
    ssd1306_command(ssd1306_init_sequence[i]);
  }

  /* vertical addressing mode */
  send_cmd2(0x20, 0x01);

  /* reverse order */
  ssd1306_command(0xc0); // 0xc8?

  /* reset start address */
  send_cmd3(0x21, 0x00, 0x7f);
  send_cmd3(0x22, 0x00, 0x07);

  /* clear screen */
  for (i = 0; i < (128 * 64) / 8; i++)
    send_data(0x00);

  /* reset start address */
  send_cmd3(0x21, 0x00, 0x7f);
  send_cmd3(0x22, 0x00, 0x07);

  /* enable display */
  ssd1306_command(0xaf); //0xaf: display on, 0xae: display off

  draw_char(0, 0, 0);
  draw_char(1, 0, 1);
  draw_char(2, 0, 2);
  draw_char(3, 0, 3);
  draw_char(4, 0, 4);
  draw_char(5, 0, 5);
  draw_char(6, 0, 6);
  draw_char(7, 0, 7);

  draw_char(0, 1, 8);
  draw_char(1, 1, 9);
  draw_char(2, 1, 10);
  draw_char(3, 1, 11);
  draw_char(4, 1, 12);
  draw_char(5, 1, 13);
  draw_char(6, 1, 14);
  draw_char(7, 1, 15);
}
