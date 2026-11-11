#ifndef __GPIO_ATMEGA4809_H__
#define __GPIO_ATMEGA4809_H__

#if defined(ATMEGA4809)

/* optimized constant gpio access using the lowest 32 bytes I/O space */
static inline void vport_sbi(volatile void *addr, uint8_t bit)
{
  asm volatile("sbi %0, %1" : : "I" (addr), "I" (bit));
}

static inline void vport_cbi(volatile void *addr, uint8_t bit)
{
  asm volatile("cbi %0, %1" : : "I" (addr), "I" (bit));
}

static inline uint8_t vport_in(volatile void *addr)
{
  uint8_t value = 0;
  asm volatile("in %0, %1" : "=r" (value) : "I" (addr));
  return value;
}

typedef enum { GPIO_PORTA, GPIO_PORTB,
	       GPIO_PORTC, GPIO_PORTD,
	       GPIO_PORTE, GPIO_PORTF } gpio_atmega4809_port_t;

typedef struct {
  gpio_atmega4809_port_t port;
  int pin;
} gpio_atmega4809_port_pin_t;

#define GPIO_ATMEGA4809(this_port, that_pin) \
  (gpio_port_pin_t) \
  { .u.atmega4809 = (gpio_atmega4809_port_pin_t) \
      { .port = this_port, .pin = that_pin }  \
  }

#define __ATMEGA4809_GPIO(addr) (void *)(addr)

static void *gpio_atmega4809_enum_to_port[] = {
  [GPIO_PORTA] = __ATMEGA4809_GPIO(0x0400), /* PORTA */
  [GPIO_PORTB] = __ATMEGA4809_GPIO(0x0420), /* PORTB */
  [GPIO_PORTC] = __ATMEGA4809_GPIO(0x0440), /* PORTC */
  [GPIO_PORTD] = __ATMEGA4809_GPIO(0x0460), /* PORTD */
  [GPIO_PORTE] = __ATMEGA4809_GPIO(0x0480), /* PORTE */
  [GPIO_PORTF] = __ATMEGA4809_GPIO(0x04a0), /* PORTF */
};

static void *gpio_atmega4809_enum_to_vport[] = {
  [GPIO_PORTA] = __ATMEGA4809_GPIO(0x00), /* VPORTA */
  [GPIO_PORTB] = __ATMEGA4809_GPIO(0x04), /* VPORTB */
  [GPIO_PORTC] = __ATMEGA4809_GPIO(0x08), /* VPORTC */
  [GPIO_PORTD] = __ATMEGA4809_GPIO(0x0c), /* VPORTD */
  [GPIO_PORTE] = __ATMEGA4809_GPIO(0x10), /* VPORTE */
  [GPIO_PORTF] = __ATMEGA4809_GPIO(0x14), /* VPORTF */
};

static inline void
gpio_atmega4809_vport_port_set(const gpio_atmega4809_port_pin_t pp,
			       const int value,
			       const int vport_offs,
			       const int port_set_offs,
			       const int port_clr_offs)
{
  volatile uint8_t *port;

  /* use atomic bitops on VPORT whenever possible */
  if (__builtin_constant_p(pp.port) && __builtin_constant_p(pp.pin)) {
    port = gpio_atmega4809_enum_to_vport[pp.port] + vport_offs;

    if (value) {
      vport_sbi(port, pp.pin);
    } else {
      vport_cbi(port, pp.pin);
    }
  } else {
    /* use slightly more costly access using PORT (still atomic) */
    port = gpio_atmega4809_enum_to_port[pp.port];

    if (value) {
      port += port_set_offs;
    } else {
      port += port_clr_offs;
    }

    *port = 1 << pp.pin;
  }
}

static inline void
gpio_atmega4809_set_value(const gpio_atmega4809_port_pin_t pp, const int value)
{
  /* atomic data register access with VPORT.OUT, PORT.OUTSET and PORT.OUTCLR */
  gpio_atmega4809_vport_port_set(pp, value, 0x01, 0x05, 0x06);
}

static inline int
gpio_atmega4809_get_value(const gpio_atmega4809_port_pin_t pp)
{
  volatile uint8_t *port;
  uint8_t value;

  /* use VPORT.IN whenever possible */
  if (__builtin_constant_p(pp.port)) {
    port = gpio_atmega4809_enum_to_vport[pp.port] + 0x02; /* VPORT.IN */
    value = vport_in(port);
  } else {
    /* use slightly more costly access using PORT.IN */
    port = gpio_atmega4809_enum_to_port[pp.port] + 0x08; /* PORT.IN */
    value = *port;
  }

  return (value >> pp.pin) & 0x01;
}

static inline void
gpio_atmega4809_set_mode(const gpio_atmega4809_port_pin_t pp,
                         const gpio_mode_t mode)
{
  /* atomic direction reg access with VPORT.DIR, PORT.DIRSET and PORT.DIRCLR */
  gpio_atmega4809_vport_port_set(pp, mode == GPIO_MODE_OUTPUT ? 1 : 0,
                                 0x00, 0x01, 0x02);
}

static inline void
gpio_atmega4809_set_direction(const gpio_atmega4809_port_pin_t pp,
                              const gpio_direction_t dir)
{
  /* atomic direction reg access with VPORT.DIR, PORT.DIRSET and PORT.DIRCLR */
  gpio_atmega4809_vport_port_set(pp, dir == GPIO_DIR_OUTPUT ? 1 : 0,
                                 0x00, 0x01, 0x02);
}

#else /* defined(ATMEGA4809) */

typedef struct {
} gpio_atmega4809_port_pin_t;

#define GPIO_ATMEGA4809(this_port, that_pin) (gpio_port_pin_t) {}

static inline void
gpio_atmega4809_set_value(const gpio_atmega4809_port_pin_t pp,
			  const int value) {}
static inline int
gpio_atmega4809_get_value(const gpio_atmega4809_port_pin_t pp) { return -1; }

static inline void
gpio_atmega4809_set_mode(const gpio_atmega4809_port_pin_t pp,
                         const gpio_mode_t mode) {}

static inline void
gpio_atmega4809_set_direction(const gpio_atmega4809_port_pin_t pp,
                              const gpio_direction_t dir) {}

#endif /* defined(ATMEGA4809) */

#endif /* __GPIO_ATMEGA4809_H__ */
