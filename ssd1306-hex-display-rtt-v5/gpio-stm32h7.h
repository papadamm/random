#ifndef __GPIO_STM32H7_H__
#define __GPIO_STM32H7_H__

#if defined(STM32H743)

typedef enum { GPIOA_PORT, GPIOB_PORT, GPIOC_PORT,
	       GPIOD_PORT, GPIOE_PORT, GPIOF_PORT,
	       GPIOG_PORT, GPIOH_PORT, GPIOI_PORT,
	       GPIOJ_PORT, GPIOK_PORT } gpio_stm32h7_port_t;

typedef struct {
  gpio_stm32h7_port_t port;
  int pin;
} gpio_stm32h7_port_pin_t;

#define GPIO_STM32H7(this_port, that_pin) \
  (gpio_port_pin_t) \
  { .u.stm32h7 = (gpio_stm32h7_port_pin_t) \
      { .port = this_port, .pin = that_pin } \
  }

typedef struct {
  uint8_t moder, otyper, ospeedr, pupdr, afr;
} gpio_stm32h7_mode_detail_t;


#define GPIO_MODE_DETAIL_STM32H7(...) \
  (gpio_mode_detail_t) \
  { .u.stm32h7 = (gpio_stm32h7_mode_detail_t) \
      { __VA_ARGS__ } \
  }

#define __STM32H7_GPIO(addr) (void *)(addr)

static void *gpio_stm32h7_enum_to_port[] = {
  [GPIOA_PORT] = __STM32H7_GPIO(0x58020000), /* GPIOA */
  [GPIOB_PORT] = __STM32H7_GPIO(0x58020400), /* GPIOB */
  [GPIOC_PORT] = __STM32H7_GPIO(0x58020800), /* GPIOC */
  [GPIOD_PORT] = __STM32H7_GPIO(0x58020c00), /* GPIOD */
  [GPIOE_PORT] = __STM32H7_GPIO(0x58021000), /* GPIOE */
  [GPIOF_PORT] = __STM32H7_GPIO(0x58021400), /* GPIOF */
  [GPIOG_PORT] = __STM32H7_GPIO(0x58021800), /* GPIOG */
  [GPIOH_PORT] = __STM32H7_GPIO(0x58021c00), /* GPIOH */
  [GPIOI_PORT] = __STM32H7_GPIO(0x58022000), /* GPIOI */
  [GPIOJ_PORT] = __STM32H7_GPIO(0x58022400), /* GPIOJ */
  [GPIOK_PORT] = __STM32H7_GPIO(0x58022800), /* GPIOK */
};

static inline void
gpio_stm32h7_set_value(const gpio_stm32h7_port_pin_t pp,
                       const int value)
{
  volatile uint32_t *port;

  port = gpio_stm32h7_enum_to_port[pp.port] + 0x18; /* BSRR */

  if (value) {
    *port = 1 << pp.pin;
  } else {
    *port = 1 << (pp.pin + 16);
  }
}

static inline int
gpio_stm32h7_get_value(const gpio_stm32h7_port_pin_t pp)
{
  volatile uint32_t *port;

  port = gpio_stm32h7_enum_to_port[pp.port] + 0x10; /* IDR */

  return (*port >> pp.pin) & 0x01;
}

extern void
gpio_stm32h7_set_mode(const gpio_stm32h7_port_pin_t pp,
                      const gpio_mode_t mode);

extern void
gpio_stm32h7_set_mode_detail(const gpio_stm32h7_port_pin_t pp,
                             const gpio_mode_t mode,
                             const gpio_stm32h7_mode_detail_t detail);

extern void
gpio_stm32h7_set_direction(const gpio_stm32h7_port_pin_t pp,
                           const gpio_direction_t dir);

extern void gpio_stm32h7_init(void);

#else /* defined(STM32H743) */

typedef struct {
} gpio_stm32h7_port_pin_t;

#define GPIO_STM32H7(this_port, that_pin) (gpio_port_pin_t) {}

typedef struct {
} gpio_stm32h7_mode_detail_t;

#define GPIO_MODE_DETAIL_STM32H7(...) (gpio_mode_detail_t) {}

static inline void
gpio_stm32h7_set_value(const gpio_stm32h7_port_pin_t pp,
                       const int value) {}
static inline int
gpio_stm32h7_get_value(const gpio_stm32h7_port_pin_t pp) { return -1; }

static inline void
gpio_stm32h7_set_mode(const gpio_stm32h7_port_pin_t pp,
                      const gpio_mode_t mode) {}

static inline void
gpio_stm32h7_set_mode_detail(const gpio_stm32h7_port_pin_t pp,
                             const gpio_mode_t mode,
                             const gpio_stm32h7_mode_detail_t detail) {}

static inline void
gpio_stm32h7_set_direction(const gpio_stm32h7_port_pin_t pp,
                           const gpio_direction_t dir) {}

static inline void gpio_stm32h7_init(void) {}

#endif /* defined(STM32H743) */

#endif /* __GPIO_STM32H7_H__ */
