#ifndef __GPIO_CH32V_H__
#define __GPIO_CH32V_H__

#if defined(CH32V203C8T6_R0_1V0) || defined(CH32V003F4P6_R0_1V1)

typedef enum { GPIO_PORTA, GPIO_PORTB, GPIO_PORTC,
	       GPIO_PORTD, GPIO_PORTE, GPIO_PORTF } gpio_ch32v_port_t;

typedef struct {
  gpio_ch32v_port_t port;
  int pin;
} gpio_ch32v_port_pin_t;

#define GPIO_CH32V(this_port, that_pin) \
  (gpio_port_pin_t) \
  { .u.ch32v = (gpio_ch32v_port_pin_t) \
      { .port = this_port, .pin = that_pin } \
  }

typedef struct {
  enum { GPIO_ANALOG_INPUT_MODE,
         GPIO_FLOATING_INPUT_MODE,
         GPIO_MODE_WITH_PULL_UP_AND_PULL_DOWN,
         GPIO_PUSH_PULL_OUTPUT_MODE,
         GPIO_OPEN_DRAIN_OUTPUT_MODE,
         GPIO_FUNC_PUSH_PULL_OUTPUT_MODE,
         GPIO_FUNC_DRAIN_OUTPUT_MODE,
         GPIO_MODE_RAW_INPUT,
         GPIO_MODE_RAW_OUTPUT,
         GPIO_MODE_RAW_FUNCTION } mode_detail;
  uint8_t raw_cnf_mode;
} gpio_ch32v_mode_detail_t;


#define GPIO_MODE_DETAIL_CH32V(detail) \
  (gpio_mode_detail_t) \
  { .u.ch32v = (gpio_ch32v_mode_detail_t) \
      { .mode_detail = detail } \
  }

#define GPIO_MODE_DETAIL_RAW_CH32V(cnf, mode, raw)	\
  (gpio_mode_detail_t) \
  { .u.ch32v = (gpio_ch32v_mode_detail_t) \
      { .mode_detail = mode, .raw_cnf_mode = raw } \
  }

#define __CH32V_GPIO(addr) (void *)(addr)

static void *gpio_ch32v_enum_to_port[] = {
  [GPIO_PORTA] = __CH32V_GPIO(0x40010800), /* PORTA */
  [GPIO_PORTB] = __CH32V_GPIO(0x40010c00), /* PORTB */
  [GPIO_PORTC] = __CH32V_GPIO(0x40011000), /* PORTC */
  [GPIO_PORTD] = __CH32V_GPIO(0x40011400), /* PORTD */
  [GPIO_PORTE] = __CH32V_GPIO(0x40011800), /* PORTE */
  [GPIO_PORTF] = __CH32V_GPIO(0x40011c00), /* PORTF */
};

static inline void
gpio_ch32v_set_value(const gpio_ch32v_port_pin_t pp,
		     const int value)
{
  volatile uint32_t *port_set = gpio_ch32v_enum_to_port[pp.port] + 0x10; /* BSHR */
  volatile uint32_t *port_clr = gpio_ch32v_enum_to_port[pp.port] + 0x14; /* BCR */

  if (value) {
    *port_set = 1 << pp.pin;
  } else {
    *port_clr = 1 << pp.pin;
  }
}

static inline int
gpio_ch32v_get_value(const gpio_ch32v_port_pin_t pp)
{
  volatile uint32_t *port = gpio_ch32v_enum_to_port[pp.port] + 0x08; /* INDR */

  return (*port >> pp.pin) & 0x01;
}

extern void
gpio_ch32v_set_mode(const gpio_ch32v_port_pin_t pp,
                    const gpio_mode_t mode);

extern void
gpio_ch32v_set_mode_detail(const gpio_ch32v_port_pin_t pp,
                           const gpio_mode_t mode,
                           const gpio_ch32v_mode_detail_t detail);

extern void
gpio_ch32v_set_direction(const gpio_ch32v_port_pin_t pp,
                         const gpio_direction_t dir);

extern void gpio_ch32v_init(void);

#else /* defined(CH32V203C8T6_R0_1V0) || defined(CH32V003F4P6_R0_1V1) */

typedef struct {
} gpio_ch32v_port_pin_t;

#define GPIO_CH32V(this_port, that_pin) (gpio_port_pin_t) {}

typedef struct {
} gpio_ch32v_mode_detail_t;

#define GPIO_MODE_DETAIL_CH32V(...) (gpio_mode_detail_t) {}
#define GPIO_MODE_DETAIL_RAW_CH32V(...) (gpio_mode_detail_t) {}

static inline void
gpio_ch32v_set_value(const gpio_ch32v_port_pin_t pp,
                     const int value) {}
static inline int
gpio_ch32v_get_value(const gpio_ch32v_port_pin_t pp) { return -1; }

static inline void
gpio_ch32v_set_mode(const gpio_ch32v_port_pin_t pp,
                    const gpio_mode_t mode) {}

static inline void
gpio_ch32v_set_mode_detail(const gpio_ch32v_port_pin_t pp,
                           const gpio_mode_t mode,
                           const gpio_ch32v_mode_detail_t detail) {}

static inline void
gpio_ch32v_set_direction(const gpio_ch32v_port_pin_t pp,
                         const gpio_direction_t dir) {}

static inline void gpio_ch32v_init(void) {}

#endif /* defined(CH32V203C8T6_R0_1V0) || defined(CH32V003F4P6_R0_1V1) */

#endif /* __GPIO_CH32V_H__ */
