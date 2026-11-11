#include <stdint.h>
#include "gpio.h"

/* I2C sequencer */
#define SEQ_SDA_L SEQ_CLEAR(0)
#define SEQ_SDA_H SEQ_SET(0)
#define SEQ_SCL_L SEQ_CLEAR(1)
#define SEQ_SCL_H SEQ_SET(1)

static void i2c_seq_start(const gpio_port_pin_t scl, const gpio_port_pin_t sda)
{
  /* generate start condition */
  gpio_seq_run(GPIO_SEQ_PINS( sda, scl ),
               GPIO_SEQ_OPCODES( SEQ_SET(0), SEQ_SET(1),
                                 SEQ_MODE_OUTPUT(0), SEQ_MODE_OUTPUT(1),
                                 /* generate start condition */
                                 SEQ_SDA_L, SEQ_BARRIER,
                                 SEQ_SCL_L, SEQ_BARRIER, ), 0, 0);
}

static int i2c_seq_out8(const gpio_port_pin_t scl, const gpio_port_pin_t sda,
                        uint8_t data)
{
  gpio_sequence_data_out_t data_out;
  int k, ack;

  data_out = GPIO_SEQ_DATA_OUT(0, 0, 0, 0, 0, 0, 0, 0);

  for (k = 0; k < 8; k++) {
    data_out.data_out[k] = data & 0x01;
    data >>= 1;
  }

  /* output 8 bits */
  gpio_seq_run(GPIO_SEQ_PINS( sda, scl ),
               GPIO_SEQ_OPCODES( /* clock out bit 7 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(7),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 6 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(6),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 5 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(5),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 4 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(4),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 3 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(3),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 2 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(2),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 1 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(1),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 /* clock out bit 0 */
                                 SEQ_SCL_L, SEQ_DATA_OUT(0),
                                 SEQ_SET_VALUE(0), SEQ_BARRIER, SEQ_SCL_H,
                                 SEQ_BARRIER, SEQ_SCL_L ), &data_out, 0);

  gpio_set_value(sda, 1);
  gpio_set_direction(sda, GPIO_DIR_INPUT);

  /* allow slave device to keep scl low for "clock stretching" */
  gpio_set_value(scl, 1);
  gpio_set_direction(scl, GPIO_DIR_INPUT);
  for (k = 0; k < 100; k++) {
    if (gpio_get_value(scl)) {
      break;
    }
  }

  ack = gpio_get_value(sda);

  /* keep SCL low in between transfers */
  gpio_set_direction(scl, GPIO_DIR_OUTPUT);
  gpio_set_value(scl, 0);

  gpio_set_direction(sda, GPIO_DIR_OUTPUT);
  return ack;
}

static void i2c_seq_in8(const gpio_port_pin_t scl, const gpio_port_pin_t sda,
                        uint8_t *data_out, int ack)
{
  gpio_sequence_data_in_t data_in;
  uint8_t data;
  int k;

  data_in = GPIO_SEQ_DATA_IN(0, 0, 0, 0, 0, 0, 0, 0);
  gpio_set_direction(sda, GPIO_DIR_INPUT);

  /* input 8 bits */
  gpio_seq_run(GPIO_SEQ_PINS( sda, scl ),
               GPIO_SEQ_OPCODES( /* clock in bit 7 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(7),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 6 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(6),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 5 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(5),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 4 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(4),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 3 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(3),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 2 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(2),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 1 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(1),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                                 /* clock in bit 0 */
                                 SEQ_SCL_H, SEQ_BARRIER, SEQ_DATA_IN(0),
                                 SEQ_GET_VALUE(0), SEQ_BARRIER, SEQ_SCL_L,
                               ), 0, &data_in);

  gpio_set_direction(sda, GPIO_DIR_OUTPUT);

  if (ack) {
    gpio_set_value(sda, 0);
  } else {
    gpio_set_value(sda, 1);
  }

  /* allow slave device to keep scl low for "clock stretching" */
  gpio_set_value(scl, 1);
  gpio_set_direction(scl, GPIO_DIR_INPUT);
  for (k = 0; k < 100; k++) {
    if (gpio_get_value(scl)) {
      break;
    }
  }

  data = 0;
  for (k = 7; k >= 0; k--) {
    data <<= 1;
    data |= data_in.data_in[k];
  }
  *data_out = data;

  /* keep SCL low in between transfers */
  gpio_set_direction(scl, GPIO_DIR_OUTPUT);
  gpio_set_value(scl, 0);
}

static void i2c_seq_stop(const gpio_port_pin_t scl, const gpio_port_pin_t sda)
{
  /* generate stop condition */
  gpio_seq_run(GPIO_SEQ_PINS( sda, scl ),
               GPIO_SEQ_OPCODES( SEQ_SDA_L, SEQ_BARRIER,
                                 SEQ_SCL_L, SEQ_BARRIER,
                                 /* generate stop condition */
                                 SEQ_SCL_H, SEQ_BARRIER,
                                 SEQ_SDA_H, SEQ_BARRIER, ), 0, 0);
}

void i2c_write(const gpio_port_pin_t scl, const gpio_port_pin_t sda,
	       uint8_t device_address, uint8_t *tx_buf, unsigned int tx_len)
{
  int k;

  i2c_seq_start(scl, sda);
  i2c_seq_out8(scl, sda, device_address);

  for (k = 0; k < tx_len; k++) {
    i2c_seq_out8(scl, sda, tx_buf[k]);
  }

  i2c_seq_stop(scl, sda);
}

void i2c_read(const gpio_port_pin_t scl, const gpio_port_pin_t sda,
	      uint8_t device_address, uint8_t *rx_buf, unsigned int rx_len)
{
  int k;

  i2c_seq_start(scl, sda);
  i2c_seq_out8(scl, sda, device_address);

  for (k = 0; k < rx_len; k++) {
    i2c_seq_in8(scl, sda, &rx_buf[k], 1);
  }

  i2c_seq_stop(scl, sda);
}
