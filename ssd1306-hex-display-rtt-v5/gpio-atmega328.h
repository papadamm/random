#ifndef __GPIO_ATMEGA328_H__
#define __GPIO_ATMEGA328_H__

#if defined(ATMEGA328P)

/* optimized constant gpio access using the lowest 32 bytes I/O space */
static inline void avr_sbi(volatile void *addr, uint8_t bit)
{
  asm volatile("sbi %0, %1" : : "I" (addr), "I" (bit));
}

static inline void avr_cbi(volatile void *addr, uint8_t bit)
{
  asm volatile("cbi %0, %1" : : "I" (addr), "I" (bit));
}

static inline uint8_t avr_in(volatile void *addr)
{
  uint8_t value = 0;
  asm volatile("in %0, %1" : "=r" (value) : "I" (addr));
  return value;
}

typedef enum { GPIO_PORTB, GPIO_PORTC, GPIO_PORTD } gpio_atmega328_port_t;

typedef struct {
  gpio_atmega328_port_t port;
  int pin;
} gpio_atmega328_port_pin_t;

#define GPIO_ATMEGA328(this_port, that_pin) \
  (gpio_port_pin_t) \
  { .u.atmega328 = (gpio_atmega328_port_pin_t) \
      { .port = this_port, .pin = that_pin }  \
  }

#define __ATMEGA328_GPIO(addr) (void *)(addr)

static void *gpio_atmega328_enum_to_port[] = {
  [GPIO_PORTB] = __ATMEGA328_GPIO(0x03), /* PINB, DDRB, PORTB */
  [GPIO_PORTC] = __ATMEGA328_GPIO(0x06), /* PINC, DDRC, PORTC */
  [GPIO_PORTD] = __ATMEGA328_GPIO(0x09), /* PIND, DDRD, PORTD */
};

static inline void
__gpio_atmega328_port_set(const gpio_atmega328_port_pin_t pp,
                          const int value,
                          const int port_offs)
{
  volatile uint8_t *port;

  /* use atomic bitops via SBI and CBI whenever possible */
  if (__builtin_constant_p(pp.port) && __builtin_constant_p(pp.pin)) {
    port = gpio_atmega328_enum_to_port[pp.port] + port_offs;

    if (value) {
      avr_sbi(port, pp.pin);
    } else {
      avr_cbi(port, pp.pin);
    }
  } else {
    /* let the compiler figure it out */
    port = gpio_atmega328_enum_to_port[pp.port] + port_offs;

    if (value) {
      *port |= 1 << pp.pin;
    } else {
      *port &= ~(1 << pp.pin);
    }
  }
}

static inline void
gpio_atmega328_set_value(const gpio_atmega328_port_pin_t pp, const int value)
{
  /* atomic data register access with PORTB, PORTC or PORTD */
  __gpio_atmega328_port_set(pp, value, 0x02);
}

static inline int
gpio_atmega328_get_value(const gpio_atmega328_port_pin_t pp)
{
  volatile uint8_t *port = gpio_atmega328_enum_to_port[pp.port]; /* PINx */
  uint8_t value;

  /* use IN instruction on PINx register whenever possible */
  if (__builtin_constant_p(pp.port)) {
    value = avr_in(port);
  } else {
    value = *port;
  }

  return (value >> pp.pin) & 0x01;
}

static inline void
gpio_atmega328_set_mode(const gpio_atmega328_port_pin_t pp,
                        const gpio_mode_t mode)
{
  /* atomic data register access with DDRB, DDRC or DDRD */
  __gpio_atmega328_port_set(pp, mode == GPIO_MODE_OUTPUT ? 1 : 0, 0x01);
}

static inline void
gpio_atmega328_set_direction(const gpio_atmega328_port_pin_t pp,
                             const gpio_direction_t dir)
{
  /* atomic data register access with DDRB, DDRC or DDRD */
  __gpio_atmega328_port_set(pp, dir == GPIO_DIR_OUTPUT ? 1 : 0, 0x01);
}

#else /* defined(ATMEGA328P) */

typedef struct {
} gpio_atmega328_port_pin_t;

#define GPIO_ATMEGA328(this_port, that_pin) (gpio_port_pin_t) {}

static inline void
gpio_atmega328_set_value(const gpio_atmega328_port_pin_t pp,
                         const int value) {}
static inline int
gpio_atmega328_get_value(const gpio_atmega328_port_pin_t pp) { return -1; }

static inline void
gpio_atmega328_set_mode(const gpio_atmega328_port_pin_t pp,
                        const gpio_mode_t mode) {}

static inline void
gpio_atmega328_set_direction(const gpio_atmega328_port_pin_t pp,
                             const gpio_direction_t dir) {}

#endif /* defined(ATMEGA328P) */

#endif /* __GPIO_ATMEGA328_H__ */
