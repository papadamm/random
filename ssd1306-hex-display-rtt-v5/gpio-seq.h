#ifndef __GPIO_SEQ_H__
#define __GPIO_SEQ_H__

// system wide compile time limitations for the sequencer
#define GPIO_SEQ_MAX_OPC 64
#define GPIO_SEQ_MAX_PINS 2
#define GPIO_SEQ_MAX_DATA_IN 8
#define GPIO_SEQ_MAX_DATA_OUT 8

typedef struct {
  // a list of GPIOs that may become associated with a certain sequence
  gpio_port_pin_t pp[GPIO_SEQ_MAX_PINS];
  int len;
} gpio_sequence_pins_t;

#define GPIO_SEQ_PINS(...) \
  (gpio_sequence_pins_t) \
  { .pp = { __VA_ARGS__ }, \
    .len = sizeof((gpio_port_pin_t[]){ __VA_ARGS__ }) \
      / sizeof(gpio_port_pin_t) \
  }

typedef enum { SEQ_OP_BARRIER,
               SEQ_OP_DATA_IN, SEQ_OP_DATA_OUT,
               SEQ_OP_SET, SEQ_OP_CLEAR,
               SEQ_OP_SET_VALUE, SEQ_OP_GET_VALUE,
               SEQ_OP_MODE_INPUT, SEQ_OP_MODE_OUTPUT,
} gpio_seq_t;

typedef struct {
  // a list of sequence opcodes, used together with sequence pins
  gpio_seq_t opc[GPIO_SEQ_MAX_OPC];
  int len;
} gpio_sequence_opcodes_t;

#define GPIO_SEQ_OPCODES(...) \
  (gpio_sequence_opcodes_t) \
  { .opc = { __VA_ARGS__ }, \
    .len = sizeof((gpio_seq_t[]){ __VA_ARGS__ }) \
      / sizeof(gpio_seq_t) \
  }

#define SEQ_ARG_MASK  0xf0
#define SEQ_ARG_SHIFT 4
#define SEQ_MASK      0x0f

#define SEQ_BARRIER        (SEQ_OP_BARRIER)
#define SEQ_SET(n)         (SEQ_OP_SET | ((n) << SEQ_ARG_SHIFT))
#define SEQ_CLEAR(n)       (SEQ_OP_CLEAR | ((n) << SEQ_ARG_SHIFT))
#define SEQ_DATA_IN(n)     (SEQ_OP_DATA_IN | ((n) << SEQ_ARG_SHIFT))
#define SEQ_DATA_OUT(n)    (SEQ_OP_DATA_OUT | ((n) << SEQ_ARG_SHIFT))
#define SEQ_SET_VALUE(n)   (SEQ_OP_SET_VALUE | ((n) << SEQ_ARG_SHIFT))
#define SEQ_GET_VALUE(n)   (SEQ_OP_GET_VALUE | ((n) << SEQ_ARG_SHIFT))
#define SEQ_MODE_INPUT(n)  (SEQ_OP_MODE_INPUT | ((n) << SEQ_ARG_SHIFT))
#define SEQ_MODE_OUTPUT(n) (SEQ_OP_MODE_OUTPUT | ((n) << SEQ_ARG_SHIFT))


typedef struct {
  // a fixed-size sequence data buffer for incoming data
  uint8_t data_in[GPIO_SEQ_MAX_DATA_IN];
  int len;
} gpio_sequence_data_in_t;

#define GPIO_SEQ_DATA_IN(...) \
  (gpio_sequence_data_in_t) \
  { .data_in = { __VA_ARGS__ }, \
    .len = sizeof((uint8_t[]){ __VA_ARGS__ }) \
      / sizeof(uint8_t) \
  }

typedef struct {
  // a fixed-size sequence data buffer for outgoing data
  uint8_t data_out[GPIO_SEQ_MAX_DATA_OUT];
  int len;
} gpio_sequence_data_out_t;

#define GPIO_SEQ_DATA_OUT(...) \
  (gpio_sequence_data_out_t) \
  { .data_out = { __VA_ARGS__ }, \
    .len = sizeof((uint8_t[]){ __VA_ARGS__ }) \
      / sizeof(uint8_t) \
  }

static inline void gpio_seq_run(gpio_sequence_pins_t seq_pins,
                                gpio_sequence_opcodes_t seq_opcodes,
                                gpio_sequence_data_out_t *seq_data_out,
                                gpio_sequence_data_in_t *seq_data_in)
{
  int k, m;
  uint8_t data_out_idx = 0;
  uint8_t data_in_idx = 0;

  for (k = 0; k < seq_opcodes.len; k++) {
    m = (seq_opcodes.opc[k] & SEQ_ARG_MASK) >> SEQ_ARG_SHIFT;

    if (seq_data_out && (seq_data_out->len > 0)) {
      switch (seq_opcodes.opc[k] & SEQ_MASK) {
      case SEQ_OP_DATA_OUT:
        data_out_idx = m;
        break;
      case SEQ_OP_SET_VALUE:
        gpio_set_value(seq_pins.pp[m],
                       seq_data_out->data_out[data_out_idx] != 0);
        break;
      }
    }
    if (seq_data_in && (seq_data_in->len > 0)) {
      switch (seq_opcodes.opc[k] & SEQ_MASK) {
      case SEQ_OP_DATA_IN:
        data_in_idx = m;
        break;
      case SEQ_OP_GET_VALUE:
        seq_data_in->data_in[data_in_idx] = \
          gpio_get_value(seq_pins.pp[m]);
        break;
      }
    }

    switch (seq_opcodes.opc[k] & SEQ_MASK) {
    case SEQ_OP_SET:
      gpio_set_value(seq_pins.pp[m], 1);
      break;
    case SEQ_OP_CLEAR:
      gpio_set_value(seq_pins.pp[m], 0);
      break;
    case SEQ_OP_MODE_INPUT:
      gpio_set_mode(seq_pins.pp[m], GPIO_MODE_INPUT);
      break;
    case SEQ_OP_MODE_OUTPUT:
      gpio_set_mode(seq_pins.pp[m], GPIO_MODE_OUTPUT);
      break;
    case SEQ_OP_BARRIER:
      /* do nothing for now */
      break;
    }
  }
}

#endif /* __GPIO_SEQ_H__ */
