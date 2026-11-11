#include <stdint.h>

#include "gpio.h"
#include "occ-pack-flush.h"

#if defined(CH32V203C8T6_R0_1V0) || defined(CH32V003F4P6_R0_1V1)

/* for each GPIO allow 3 settings with index GPIO_MODE_xx */
/* details may be set with GPIO_MODE_RAW_xxx */
typedef struct {
  uint8_t idx;
  uint8_t data[3];
} gpio_ch32v_unpacked_per_gpio_data_t;

typedef struct {
  struct occ_pack_flush_head head;
  gpio_ch32v_unpacked_per_gpio_data_t unpacked[8]; /* 8 GPIOs per CFGxR register */
  gpio_ch32v_port_pin_t pp;
  volatile uint32_t *port;
} gpio_ch32v_cfg_unpacked_t;

typedef uint32_t gpio_ch32v_cfg_packed_t;

static gpio_ch32v_cfg_unpacked_t gpio_ch32v_cfglr_occ[] = {
  [GPIO_PORTA] = {},
  [GPIO_PORTB] = {},
  [GPIO_PORTC] = {},
  [GPIO_PORTD] = {},
  [GPIO_PORTE] = {},
  [GPIO_PORTF] = {},
};

static gpio_ch32v_cfg_unpacked_t gpio_ch32v_cfghr_occ[] = {
  [GPIO_PORTA] = {},
  [GPIO_PORTB] = {},
  [GPIO_PORTC] = {},
  [GPIO_PORTD] = {},
  [GPIO_PORTE] = {},
  [GPIO_PORTF] = {},
};

#define head_to_unpacked(h) \
  (gpio_ch32v_cfg_unpacked_t *) \
  ((void *)(h) - __builtin_offsetof(gpio_ch32v_cfg_unpacked_t, head))

static void gpio_ch32v_occ_init(struct occ_pack_flush_head *head)
{
  gpio_ch32v_cfg_unpacked_t *cfg = head_to_unpacked(head);
  uint32_t packed;
  int k;

  /* initialize unpacked data structure with defaults for each mode */
  for (k = 0; k < 8; k++) {
    cfg->unpacked[k].data[0] = 0x04; /* INPUT: Floating input mode */
    cfg->unpacked[k].data[1] = 0x06; /* OUTPUT: Open-Drain 2 MHz */
    cfg->unpacked[k].data[2] = 0xc0; /* FUNCTION: Reserved */
  }

  /* figure out which hardware register we should access */
  if (cfg->pp.pin <= 7) {
    cfg->port = gpio_ch32v_enum_to_port[cfg->pp.port] + 0x0; /* CFGLR */
  } else {
    cfg->port = gpio_ch32v_enum_to_port[cfg->pp.port] + 0x4; /* CFGHR */
  }

  /* read from hardware CFGxR register (a single 32-bit read access) */
  packed = *cfg->port;

  /* update unpacked data structure with current information from hardware */
  for (k = 0; k < 8; k++) {
    switch (packed & 0x03) {
    case 0:
      switch (packed & 0x0c) {
      case 0x00:
      case 0x04: /* Input mode (Analog or Floating) */
	cfg->unpacked[k].idx = 0;
	cfg->unpacked[k].data[0] = packed & 0x0f;
	break;
      case 0x08:
      case 0x0c: /* Function (With Pull-Up/Down and Reserved) */
	cfg->unpacked[k].idx = 2;
	cfg->unpacked[k].data[2] = packed & 0x0f;
	break;
      }
      break;

    default:
      switch (packed & 0x0c) {
      case 0x00:
      case 0x04: /* Output mode (Push-Pull or Open-Drain) */
	cfg->unpacked[k].idx = 1;
	cfg->unpacked[k].data[1] = packed & 0x0f;
	break;
      case 0x08:
      case 0x0c: /* Function (With Pull-Up/Down and Reserved) */
	cfg->unpacked[k].idx = 2;
	cfg->unpacked[k].data[2] = packed & 0x0f;
	break;
      }
      break;
    }

    packed >>= 4;
  }
}

static void gpio_ch32v_occ_pack(struct occ_pack_flush_head *head, void *pack_out)
{
  gpio_ch32v_cfg_unpacked_t *cfg = head_to_unpacked(head);
  uint32_t *packed_ptr = pack_out;
  gpio_ch32v_unpacked_per_gpio_data_t *unpacked_ptr;
  uint8_t unpacked[8] = {};
  uint32_t packed;
  int k;

  for (k = 0; k < 8; k++) {
    unpacked_ptr = &cfg->unpacked[k];
    switch (unpacked_ptr->idx) {
    case 0:
      unpacked[k] = unpacked_ptr->data[0];
      break;

    case 1:
      unpacked[k] = unpacked_ptr->data[1];
      break;

    case 2:
      unpacked[k] = unpacked_ptr->data[2];
      break;
    }
  }

  packed = 0;

  for (k = 7; k >= 0; k--) {
    packed <<= 4;
    packed |= unpacked[k];
  }

  *packed_ptr = packed;
}

static void gpio_ch32v_occ_flush(struct occ_pack_flush_head *head, void *pack_in)
{
  gpio_ch32v_cfg_unpacked_t *cfg = head_to_unpacked(head);
  uint32_t *packed_ptr = pack_in;

  /* write to hardware CFGxR register (a single 32-bit write access) */
  *cfg->port = *packed_ptr;
}

static int gpio_ch32v_occ_has_changed(struct occ_pack_flush_head *head,
				      void *pack_a, void *pack_b)
{
  uint32_t *packed_ptr_a = (uint32_t *)pack_a;
  uint32_t *packed_ptr_b = (uint32_t *)pack_b;

  return *packed_ptr_a != *packed_ptr_b;
}

void gpio_ch32v_set_mode_detail(const gpio_ch32v_port_pin_t pp,
                                const gpio_mode_t mode,
                                const gpio_ch32v_mode_detail_t detail)
{
  gpio_ch32v_cfg_packed_t packed_a, packed_b;
  uint8_t default_output_bits = 0x1; /* 10 MHz on CH32V003 and CH32V2003 */
  gpio_ch32v_unpacked_per_gpio_data_t *unpacked_ptr;
  int index = -1;
  uint8_t cnf_mode;

  if (pp.pin <= 7) {
    unpacked_ptr = &gpio_ch32v_cfglr_occ[pp.port].unpacked[pp.pin];
  } else {
    unpacked_ptr = &gpio_ch32v_cfghr_occ[pp.port].unpacked[pp.pin - 8];
  }

  switch(mode) {
  case GPIO_MODE_INPUT:
    unpacked_ptr->idx = 0;
    break;

  case GPIO_MODE_OUTPUT:
    unpacked_ptr->idx = 1;
    break;

  case GPIO_MODE_FUNCTION:
    unpacked_ptr->idx = 2;
    break;
  }

  switch(detail.mode_detail) {
  case GPIO_ANALOG_INPUT_MODE:
    index = GPIO_MODE_INPUT;
    cnf_mode = 0x0 << 2;
    break;

  case GPIO_FLOATING_INPUT_MODE:
    index = GPIO_MODE_INPUT;
    cnf_mode = 0x1 << 2;
    break;

  case GPIO_MODE_WITH_PULL_UP_AND_PULL_DOWN:
    index = GPIO_MODE_FUNCTION;
    cnf_mode = 0x2 << 2;
    break;

  case GPIO_PUSH_PULL_OUTPUT_MODE:
    index = GPIO_MODE_OUTPUT;
    cnf_mode = (0x0 << 2) | default_output_bits;
    break;

  case GPIO_OPEN_DRAIN_OUTPUT_MODE:
    index = GPIO_MODE_OUTPUT;
    cnf_mode = (0x1 << 2) | default_output_bits;
    break;

  case GPIO_FUNC_PUSH_PULL_OUTPUT_MODE:
    index = GPIO_MODE_FUNCTION;
    cnf_mode = (0x2 << 2) | default_output_bits;
    break;

  case GPIO_FUNC_DRAIN_OUTPUT_MODE:
    index = GPIO_MODE_FUNCTION;
    cnf_mode = (0x3 << 2) | default_output_bits;
    break;

  case GPIO_MODE_RAW_INPUT:
    index = GPIO_MODE_INPUT;
    cnf_mode = detail.raw_cnf_mode;
    break;

  case GPIO_MODE_RAW_OUTPUT:
    index = GPIO_MODE_OUTPUT;
    cnf_mode = detail.raw_cnf_mode;
    break;

  case GPIO_MODE_RAW_FUNCTION:
    index = GPIO_MODE_FUNCTION;
    cnf_mode = detail.raw_cnf_mode;
    break;
  }

  switch (index) {
  case GPIO_MODE_INPUT:
    unpacked_ptr->data[0] = cnf_mode;
    break;

  case GPIO_MODE_OUTPUT:
    unpacked_ptr->data[1] = cnf_mode;
    break;

  case GPIO_MODE_FUNCTION:
    unpacked_ptr->data[2] = cnf_mode;
    break;
  }

  if (pp.pin <= 7) {
    occ_pack_flush(&gpio_ch32v_cfglr_occ[pp.port].head, &packed_a, &packed_b);
  } else {
    occ_pack_flush(&gpio_ch32v_cfghr_occ[pp.port].head, &packed_a, &packed_b);
  }
}

static void __set_mode_detail_ext(const gpio_ch32v_port_pin_t pp,
                                  const gpio_mode_t mode,
                                  const gpio_mode_detail_t detail)
{
  gpio_ch32v_set_mode_detail(pp, mode, detail.u.ch32v);
}

void gpio_ch32v_set_mode(const gpio_ch32v_port_pin_t pp,
			 const gpio_mode_t mode)
{
  /* here we simply select fixed defaults with each mode to use */

  switch(mode) {
  case GPIO_MODE_INPUT:
    __set_mode_detail_ext(pp, mode,
                          GPIO_MODE_DETAIL_CH32V(GPIO_FLOATING_INPUT_MODE));
    break;

  case GPIO_MODE_OUTPUT:
    __set_mode_detail_ext(pp, mode,
                          GPIO_MODE_DETAIL_CH32V(GPIO_PUSH_PULL_OUTPUT_MODE));
    break;

  case GPIO_MODE_FUNCTION:
    __set_mode_detail_ext(pp, mode,
                          GPIO_MODE_DETAIL_CH32V(GPIO_FUNC_DRAIN_OUTPUT_MODE));
    break;
  }
}

void gpio_ch32v_set_direction(const gpio_ch32v_port_pin_t pp,
                              const gpio_direction_t dir)
{
  gpio_ch32v_cfg_packed_t packed_a, packed_b;
  gpio_ch32v_unpacked_per_gpio_data_t *unpacked;

  if (pp.pin <= 7) {
    unpacked = &gpio_ch32v_cfglr_occ[pp.port].unpacked[pp.pin];
  } else {
    unpacked = &gpio_ch32v_cfghr_occ[pp.port].unpacked[pp.pin - 8];
  }

  switch(dir) {
  case GPIO_DIR_INPUT:
    unpacked->idx = 0;
    break;

  case GPIO_DIR_OUTPUT:
    unpacked->idx = 1;
    break;
  }

  if (pp.pin <= 7) {
    occ_pack_flush(&gpio_ch32v_cfglr_occ[pp.port].head, &packed_a, &packed_b);
  } else {
    occ_pack_flush(&gpio_ch32v_cfghr_occ[pp.port].head, &packed_a, &packed_b);
  }
}

struct occ_pack_flush_ops gpio_ch32_occ_pack_flush_ops = {
  .init = gpio_ch32v_occ_init,
  .pack = gpio_ch32v_occ_pack,
  .flush = gpio_ch32v_occ_flush,
  .has_changed = gpio_ch32v_occ_has_changed,
};

static void gpio_ch32v_init_occ_one(gpio_ch32v_cfg_unpacked_t *cfg,
                                    gpio_ch32v_port_pin_t pp)
{
  cfg->pp = pp;
  occ_pack_flush_init(&cfg->head, &gpio_ch32_occ_pack_flush_ops);
}

static void gpio_ch32v_init_cfglr_cfghr(gpio_port_pin_t gpio_cfglr,
					gpio_port_pin_t gpio_cfghr)
{
  gpio_ch32v_init_occ_one(&gpio_ch32v_cfglr_occ[gpio_cfglr.u.ch32v.port],
                          gpio_cfglr.u.ch32v);
  gpio_ch32v_init_occ_one(&gpio_ch32v_cfghr_occ[gpio_cfghr.u.ch32v.port],
                          gpio_cfghr.u.ch32v);
}

void gpio_ch32v_init(void)
{
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTA, 0),
			      GPIO_CH32V(GPIO_PORTA, 8));
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTB, 0),
			      GPIO_CH32V(GPIO_PORTB, 8));
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTC, 0),
			      GPIO_CH32V(GPIO_PORTC, 8));
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTD, 0),
			      GPIO_CH32V(GPIO_PORTD, 8));
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTE, 0),
			      GPIO_CH32V(GPIO_PORTE, 8));
  gpio_ch32v_init_cfglr_cfghr(GPIO_CH32V(GPIO_PORTF, 0),
			      GPIO_CH32V(GPIO_PORTF, 8));
}

#endif /* defined(CH32V203C8T6_R0_1V0) || defined(CH32V003F4P6_R0_1V1) */
