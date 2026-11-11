#include <stdint.h>

#include "gpio.h"
#include "rtt/rtt.h"

#ifdef SEEEDUINO_NANO
#define LED_GPIO_PIN GPIO_ATMEGA328(GPIO_PORTB, 5)
#endif

#ifdef ARDUINO_NANO_EVERY
#define LED_GPIO_PIN GPIO_ATMEGA4809(GPIO_PORTE, 2)
#endif

#if defined(PIXHAWK_6C_STM32H743)
#define LED_GPIO_PIN GPIO_STM32H7(GPIOD_PORT, 10)
#endif

#if defined(PIXHAWK_6C_STM32F103)
#define LED_GPIO_PIN GPIO_STM32F1(GPIOB_PORT, 14)
#endif

#ifdef CH32V003F4P6_R0_1V1
#define LED_GPIO_PIN GPIO_CH32V(GPIO_PORTC, 0)
#endif

#ifdef CH32V203C8T6_R0_1V0
#define LED_GPIO_PIN GPIO_CH32V(GPIO_PORTC, 13)
#define LED_GPIO_PIN_MODE_DETAIL \
  GPIO_MODE_DETAIL_CH32V(GPIO_PUSH_PULL_OUTPUT_MODE)
#endif

#ifndef LED_GPIO_PIN
#define LED_GPIO_PIN GPIO_NONE
#endif

void led_setup(void)
{
#ifdef LED_GPIO_PIN_MODE_DETAIL
  gpio_set_mode_detail(LED_GPIO_PIN, GPIO_MODE_OUTPUT, LED_GPIO_PIN_MODE_DETAIL);
#else
  gpio_set_direction(LED_GPIO_PIN, GPIO_DIR_OUTPUT);
#endif
}

void led_on_set_value(void)
{
  gpio_set_value(LED_GPIO_PIN, 1);
}

void led_off_set_value(void)
{
  gpio_set_value(LED_GPIO_PIN, 0);
}

void led_on_seq(void)
{
  gpio_seq_run(GPIO_SEQ_PINS( LED_GPIO_PIN ),
	       GPIO_SEQ_OPCODES( SEQ_SET(0) ), 0, 0);
}

void led_off_seq(void)
{
  gpio_seq_run(GPIO_SEQ_PINS( LED_GPIO_PIN ),
	       GPIO_SEQ_OPCODES( SEQ_CLEAR(0) ), 0, 0);
}

void led_flash(void)
{
  gpio_seq_run(GPIO_SEQ_PINS( LED_GPIO_PIN ),
               GPIO_SEQ_OPCODES( SEQ_CLEAR(0), SEQ_SET(0),
                                 SEQ_CLEAR(0), SEQ_SET(0),
                                 SEQ_CLEAR(0), SEQ_SET(0),
                                 SEQ_CLEAR(0), SEQ_SET(0) ), 0, 0);
}

static void silly_delay(uint32_t value)
{
  uint32_t i;

  for (i = 0; i < value; i++) {
    asm volatile ("nop");
  }
}

void hack_i2c_init(void);
void test_display();

// example sequence of OpenOCD commands to enable RTT on the host
// (after starting the RTT server use telnet in a separate terminal)
// > rtt server start 9090 0
// > rtt setup 0x24000000 0x60000 "XXXxxx xxxXXX"
// > rtt start
// (use hayley get-memory-base and get-memory-size for rtt setup parameters)

int main(void)
{
  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "rtt debugging started: " __DATE__ " " __TIME__ "\n");

#if defined(STM32H743)
  {
    volatile uint32_t *RCC_AHB4ENR = (void *)0x580244e0;
    *RCC_AHB4ENR = 0x7ff; /* enable GPIOA->GPIOK */
  }
  gpio_stm32h7_init();
#endif
#if defined(STM32F103)
  {
    volatile uint32_t *RCC_AP24ENR = (void *)0x40021018;
    *RCC_AP24ENR = 0x1fd; /* enable AFIO and GPIOA->GPIOG */
  }
  gpio_stm32f1_init();
#endif
#if defined(CH32V003F4P6_R0_1V1)
  {
    volatile uint32_t *RCC_APB2PCENR = (void *)0x40021018;
    *RCC_APB2PCENR = 0x10;  /* IOPCEN=1 */
  }
  gpio_ch32v_init();
#endif

  led_setup();

  hack_i2c_init();
  test_display();
  
  while (1) {
    led_on_set_value();
    silly_delay(0x100000);
    led_off_set_value();
    silly_delay(0x100000);
    led_on_seq();
    silly_delay(0x100000);
    led_off_seq();
    silly_delay(0x100000);
    led_flash();
    silly_delay(0x100000);
  }

  return 0;
}
