#ifndef __GPIO_STM32F1_H__
#define __GPIO_STM32F1_H__

#if defined(STM32F103)

typedef enum { GPIOA_PORT, GPIOB_PORT, GPIOC_PORT,
	       GPIOD_PORT, GPIOE_PORT } gpio_stm32f1_port_t;

typedef struct {
  gpio_stm32f1_port_t port;
  int pin;
} gpio_stm32f1_port_pin_t;

#define GPIO_STM32F1(this_port, that_pin) \
  (gpio_port_pin_t) \
  { .u.stm32f1 = (gpio_stm32f1_port_pin_t) \
      { .port = this_port, .pin = that_pin } \
  }

typedef struct {
  uint8_t cnf, mode, odr;
} gpio_stm32f1_mode_detail_t;

#define GPIO_MODE_DETAIL_STM32F1(...) \
  (gpio_mode_detail_t) \
  { .u.stm32f1 = (gpio_stm32f1_mode_detail_t) \
      { __VA_ARGS__ } \
  }

#define __STM32F1_GPIO(addr) (void *)(addr)

static void *gpio_stm32f1_enum_to_port[] = {
  [GPIOA_PORT] = __STM32F1_GPIO(0x40010800), /* GPIOA */
  [GPIOB_PORT] = __STM32F1_GPIO(0x40010c00), /* GPIOB */
  [GPIOC_PORT] = __STM32F1_GPIO(0x40011000), /* GPIOC */
  [GPIOD_PORT] = __STM32F1_GPIO(0x40011400), /* GPIOD */
  [GPIOE_PORT] = __STM32F1_GPIO(0x40011800), /* GPIOE */
};

static inline void
gpio_stm32f1_set_value(const gpio_stm32f1_port_pin_t pp,
                       const int value)
{
  volatile uint32_t *port;

  if (value) {
    port = gpio_stm32f1_enum_to_port[pp.port] + 0x10; /* BSRR */
  } else {
    port = gpio_stm32f1_enum_to_port[pp.port] + 0x14; /* BRR */
  }

  *port = 1 << pp.pin;
}

static inline int
gpio_stm32f1_get_value(const gpio_stm32f1_port_pin_t pp)
{
  volatile uint32_t *port;

  port = gpio_stm32f1_enum_to_port[pp.port] + 0x08; /* IDR */

  return (*port >> pp.pin) & 0x01;
}

extern void
gpio_stm32f1_set_mode(const gpio_stm32f1_port_pin_t pp,
                      const gpio_mode_t mode);

extern void
gpio_stm32f1_set_mode_detail(const gpio_stm32f1_port_pin_t pp,
                             const gpio_mode_t mode,
                             const gpio_stm32f1_mode_detail_t detail);

extern void
gpio_stm32f1_set_direction(const gpio_stm32f1_port_pin_t pp,
                           const gpio_direction_t dir);

extern void gpio_stm32f1_init(void);

#else  /* defined(STM32F103) */

typedef struct {
} gpio_stm32f1_port_pin_t;

#define GPIO_STM32F1(this_port, that_pin) (gpio_port_pin_t) {}

typedef struct {
} gpio_stm32f1_mode_detail_t;

#define GPIO_MODE_DETAIL_STM32F1(...) (gpio_mode_detail_t) {}

static inline void
gpio_stm32f1_set_value(const gpio_stm32f1_port_pin_t pp,
                       const int value) {}
static inline int
gpio_stm32f1_get_value(const gpio_stm32f1_port_pin_t pp) { return -1; }

static inline void
gpio_stm32f1_set_mode(const gpio_stm32f1_port_pin_t pp,
                      const gpio_mode_t mode) {}

static inline void
gpio_stm32f1_set_mode_detail(const gpio_stm32f1_port_pin_t pp,
                             const gpio_mode_t mode,
                             const gpio_stm32f1_mode_detail_t detail) {}

static inline void
gpio_stm32f1_set_direction(const gpio_stm32f1_port_pin_t pp,
                           const gpio_direction_t dir) {}

static inline void gpio_stm32f1_init(void) {}

#endif /* defined(STM32F103) */

#endif /* __GPIO_STM32F1_H__ */
