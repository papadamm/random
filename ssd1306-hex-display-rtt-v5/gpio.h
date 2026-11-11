#ifndef __GPIO_H__
#define __GPIO_H__

typedef enum { GPIO_DIR_INPUT, GPIO_DIR_OUTPUT } gpio_direction_t;
typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_FUNCTION } gpio_mode_t;

// gpio headers for all supported systems are included here
#include "gpio-atmega328.h"
#include "gpio-atmega4809.h"
#include "gpio-ch32v.h"
#include "gpio-stm32f1.h"
#include "gpio-stm32h7.h"

typedef struct {
  union {
    // a list of private per-uC definitions of how to describe a GPIO
    gpio_atmega328_port_pin_t atmega328;
    gpio_atmega4809_port_pin_t atmega4809;
    gpio_ch32v_port_pin_t ch32v;
    gpio_stm32f1_port_pin_t stm32f1;
    gpio_stm32h7_port_pin_t stm32h7;
    // make the structure at least 1 byte large to avoid div-by-zero
    uint8_t something_rather_than_nothing;
  } u;
} gpio_port_pin_t;

typedef struct {
  union {
    // a list of private per-uC definitions of detailed mode settings
    gpio_ch32v_mode_detail_t ch32v;
    gpio_stm32f1_mode_detail_t stm32f1;
    gpio_stm32h7_mode_detail_t stm32h7;
    // make the structure at least 1 byte large to avoid div-by-zero
    uint8_t something_rather_than_nothing;
  } u;
} gpio_mode_detail_t;

// function to set a GPIO to a certain value (write the state of a GPIO pin)
// this simply goes through all the implementations
// the compiler will exclude not needed variants
static inline void gpio_set_value(const gpio_port_pin_t pp,
                                  const int value)
{
  gpio_atmega328_set_value(pp.u.atmega328, value);
  gpio_atmega4809_set_value(pp.u.atmega4809, value);
  gpio_ch32v_set_value(pp.u.ch32v, value);
  gpio_stm32f1_set_value(pp.u.stm32f1, value);
  gpio_stm32h7_set_value(pp.u.stm32h7, value);
}

// function to get GPIO value (read the state of a GPIO pin)
// this simply goes through all the implementations
// the compiler will exclude not needed variants
static inline int gpio_get_value(const gpio_port_pin_t pp)
{
  int value = -1;
  value == -1 ? value = gpio_atmega328_get_value(pp.u.atmega328) : value;
  value == -1 ? value = gpio_atmega4809_get_value(pp.u.atmega4809) : value;
  value == -1 ? value = gpio_ch32v_get_value(pp.u.ch32v) : value;
  value == -1 ? value = gpio_stm32f1_get_value(pp.u.stm32f1) : value;
  value == -1 ? value = gpio_stm32h7_get_value(pp.u.stm32h7) : value;
  return value;
}

// function to set GPIO mode (select between GPIO input, output or function)
// this simply goes through all the implementations
// the compiler will exclude not needed variants
static inline void gpio_set_mode(const gpio_port_pin_t pp, const gpio_mode_t mode)
{
  gpio_atmega328_set_mode(pp.u.atmega328, mode);
  gpio_atmega4809_set_mode(pp.u.atmega4809, mode);
  gpio_ch32v_set_mode(pp.u.ch32v, mode);
  gpio_stm32f1_set_mode(pp.u.stm32f1, mode);
  gpio_stm32h7_set_mode(pp.u.stm32h7, mode);
}

// function to set GPIO mode detail (provide more detailed information)
// this simply goes through all the implementations
// the compiler will exclude not needed variants
static inline void gpio_set_mode_detail(const gpio_port_pin_t pp,
                                        const gpio_mode_t mode,
                                        const gpio_mode_detail_t detail)
{
  gpio_ch32v_set_mode_detail(pp.u.ch32v, mode, detail.u.ch32v);
  gpio_stm32f1_set_mode_detail(pp.u.stm32f1, mode, detail.u.stm32f1);
  gpio_stm32h7_set_mode_detail(pp.u.stm32h7, mode, detail.u.stm32h7);
}

// function to set GPIO direction (select between GPIO input or output)
static inline void gpio_set_direction(const gpio_port_pin_t pp,
                                      const gpio_direction_t dir)
{
  gpio_atmega328_set_direction(pp.u.atmega328, dir);
  gpio_atmega4809_set_direction(pp.u.atmega4809, dir);
  gpio_ch32v_set_direction(pp.u.ch32v, dir);
  gpio_stm32f1_set_direction(pp.u.stm32f1, dir);
  gpio_stm32h7_set_direction(pp.u.stm32h7, dir);
}

// the GPIO implementation comes with a sequencer
#include "gpio-seq.h"

// used to compile in a missing GPIO definition
#define GPIO_NONE (gpio_port_pin_t)  (gpio_port_pin_t) {}

#endif /* __GPIO_H__ */
