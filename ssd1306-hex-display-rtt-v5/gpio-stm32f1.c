#include <stdint.h>

#include "gpio.h"
#include "occ-pack-flush.h"

#if defined(STM32F103)

/* for each GPIO pin allow 3 sets of detail with index GPIO_MODE_xx */
typedef struct {
  uint8_t idx;
  gpio_stm32f1_mode_detail_t detail[3];
} gpio_stm32f1_unpacked_per_gpio_data_t;

typedef struct {
  struct occ_pack_flush_head head;
  gpio_stm32f1_port_t port;
  gpio_stm32f1_unpacked_per_gpio_data_t unpacked[16];
} gpio_stm32f1_cfg_unpacked_t;

typedef struct {
  uint32_t crl, crh, odr;
} gpio_stm32f1_cfg_packed_t;

static gpio_stm32f1_cfg_unpacked_t gpio_stm32f1_occ[] = {
  [GPIOA_PORT] = {}, [GPIOB_PORT] = {}, [GPIOC_PORT] = {},
  [GPIOD_PORT] = {}, [GPIOE_PORT] = {},
};

#define head_to_unpacked(h) \
  (gpio_stm32f1_cfg_unpacked_t *) \
  ((void *)(h) - __builtin_offsetof(gpio_stm32f1_cfg_unpacked_t, head))

static void gpio_stm32f1_occ_init(struct occ_pack_flush_head *head)
{
  gpio_stm32f1_cfg_unpacked_t *cfg = head_to_unpacked(head);
  gpio_stm32f1_cfg_packed_t packed = {};
  gpio_stm32f1_mode_detail_t *detail;
  volatile uint32_t *base;
  int k;

  /* when building the unpacked data, clear per-pin unpacked data */
  for (k = 0; k < 16; k++) {
    memset(&cfg->unpacked[k], 0,
           sizeof(gpio_stm32f1_unpacked_per_gpio_data_t));
  }

  /* figure out base address of the GPIO controller */
  base = gpio_stm32f1_enum_to_port[cfg->port];

  /* read out GPIO hardware state into temporary local packed data */
  packed.crl = base[0]; /* 0x00: CRL */
  packed.crh = base[1]; /* 0x04: CRH */
  packed.odr = base[3]; /* 0x0C: ODR */

  /* for each pin, begin by shifting read out packed data into detail[0] */
  for (k = 0; k < 16; k++) {
    detail = &cfg->unpacked[k].detail[0];

    detail->odr = packed.odr & 0x1;
    packed.odr >>= 1;

    if (k < 8) {
      detail->mode = packed.crl & 0x3;
      packed.crl >>= 2;
      detail->cnf = packed.crl & 0x3;
      packed.crl >>= 2;
    } else {
      detail->mode = packed.crh & 0x3;
      packed.crh >>= 2;
      detail->cnf = packed.crh & 0x3;
      packed.crh >>= 2;
    }

    switch (detail->mode | (detail->cnf << 2)) {
    case 0x04:
    case 0x08:
      cfg->unpacked[k].idx = GPIO_MODE_INPUT;
      break;

    case 0x01:
    case 0x02:
    case 0x03:
    case 0x05:
    case 0x06:
    case 0x07:
      cfg->unpacked[k].idx = GPIO_MODE_OUTPUT;
      break;

    case 0x00: /* ANALOG is treated as a kind of GPIO_MODE_FUNCTION */
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0d:
    case 0x0e:
    case 0x0f:
    default: /* Treat Reserved as a kind of function as well */
      cfg->unpacked[k].idx = GPIO_MODE_FUNCTION;
      break;
    }
  }

  /* copy detail[0] to detail[1] and detail[2] */
  for (k = 0; k < 16; k++) {
    memcpy(&cfg->unpacked[k].detail[1], &cfg->unpacked[k].detail[0],
	   sizeof(*detail));
    memcpy(&cfg->unpacked[k].detail[2], &cfg->unpacked[k].detail[0],
	   sizeof(*detail));
  }
}

static void gpio_stm32f1_occ_pack(struct occ_pack_flush_head *head,
                                  void *pack_out)
{
  gpio_stm32f1_cfg_unpacked_t *cfg = head_to_unpacked(head);
  gpio_stm32f1_cfg_packed_t *packed = pack_out;
  gpio_stm32f1_mode_detail_t *detail;
  int k;

  /* when building the packed data, start from scratch and clear data */
  memset(packed, 0, sizeof(*packed));

  /* for each pin, shift unpacked input into packed copy of every register */
  for (k = 15; k >= 0; k--) {
    detail = &cfg->unpacked[k].detail[cfg->unpacked[k].idx];

    if (k >= 8) {
      packed->crh <<= 2;
      packed->crh |= detail->cnf;
      packed->crh <<= 2;
      packed->crh |= detail->mode;
    } else {
      packed->crl <<= 2;
      packed->crl |= detail->cnf;
      packed->crl <<= 2;
      packed->crl |= detail->mode;
    }

    packed->odr <<= 1;
    packed->odr |= detail->odr;
  }
}

static void gpio_stm32f1_occ_flush(struct occ_pack_flush_head *head,
                                   void *pack_in)
{
  gpio_stm32f1_cfg_unpacked_t *cfg = head_to_unpacked(head);
  gpio_stm32f1_cfg_packed_t *packed = pack_in;
  volatile uint32_t *base;

  /* figure out base address of the GPIO controller */
  base = gpio_stm32f1_enum_to_port[cfg->port];

  /* write packed data to GPIO hardware registers */
  base[0] = packed->crl; /* 0x00: CRL */
  base[1] = packed->crh; /* 0x04: CRH */
  base[3] = packed->odr; /* 0x0C: ODR */
}

static int gpio_stm32f1_occ_has_changed(struct occ_pack_flush_head *head,
                                        void *pack_a, void *pack_b)
{
  gpio_stm32f1_cfg_packed_t *packed_a = pack_a;
  gpio_stm32f1_cfg_packed_t *packed_b = pack_b;

  return memcmp(packed_a, packed_b, sizeof(*packed_a)) != 0;
}

void gpio_stm32f1_set_mode_detail(const gpio_stm32f1_port_pin_t pp,
                                  const gpio_mode_t mode,
                                  const gpio_stm32f1_mode_detail_t detail)
{
  gpio_stm32f1_cfg_packed_t packed_a, packed_b;
  gpio_stm32f1_unpacked_per_gpio_data_t *unpacked;

  unpacked = &gpio_stm32f1_occ[pp.port].unpacked[pp.pin];

  /* use idx to determine which set of details to update */
  switch(mode) {
  case GPIO_MODE_INPUT:
    unpacked->idx = 0;
    break;

  case GPIO_MODE_OUTPUT:
    unpacked->idx = 1;
    break;

  case GPIO_MODE_FUNCTION:
    unpacked->idx = 2;
    break;
  }

  /* then simply copy the settings into the unpacked per-mode data area */
  unpacked->detail[unpacked->idx] = detail;
  occ_pack_flush(&gpio_stm32f1_occ[pp.port].head, &packed_a, &packed_b);
}

static void gpio_stm32f1_set_mode_detail_ext(const gpio_stm32f1_port_pin_t pp,
                                             const gpio_mode_t mode,
                                             const gpio_mode_detail_t detail)
{
  gpio_stm32f1_set_mode_detail(pp, mode, detail.u.stm32f1);
}

void gpio_stm32f1_set_mode(const gpio_stm32f1_port_pin_t pp,
                           const gpio_mode_t mode)
{
  /* here we simply select fixed defaults with each mode to use */

  switch(mode) {
  case GPIO_MODE_INPUT: /* Floating input, pull-up or pull-down disabled */
    gpio_stm32f1_set_mode_detail_ext(pp, mode, /* CNF = 1, MODE = 0 */
                                         GPIO_MODE_DETAIL_STM32F1(0x01, 0x00));
    break;

  case GPIO_MODE_OUTPUT: /* General Purpose output push-pull */
    gpio_stm32f1_set_mode_detail_ext(pp, mode, /* CNF = 0, MODE = 3 */
                                         GPIO_MODE_DETAIL_STM32F1(0x00, 0x03));
    break;

  case GPIO_MODE_FUNCTION: /* Alternate function output Push-pull */
    gpio_stm32f1_set_mode_detail_ext(pp, mode, /* CNF = 2, MODE = 3 */
                                         GPIO_MODE_DETAIL_STM32F1(0x02, 0x03));
    break;
  }
}

void gpio_stm32f1_set_direction(const gpio_stm32f1_port_pin_t pp,
                                const gpio_direction_t dir)
{
  gpio_stm32f1_cfg_packed_t packed_a, packed_b;
  gpio_stm32f1_unpacked_per_gpio_data_t *unpacked;

  unpacked = &gpio_stm32f1_occ[pp.port].unpacked[pp.pin];

  switch(dir) {
  case GPIO_DIR_INPUT:
    unpacked->idx = 0;
    break;

  case GPIO_DIR_OUTPUT:
    unpacked->idx = 1;
    break;
  }

  occ_pack_flush(&gpio_stm32f1_occ[pp.port].head, &packed_a, &packed_b);
}

struct occ_pack_flush_ops gpio_stm32f1_occ_pack_flush_ops = {
  .init = gpio_stm32f1_occ_init,
  .pack = gpio_stm32f1_occ_pack,
  .flush = gpio_stm32f1_occ_flush,
  .has_changed = gpio_stm32f1_occ_has_changed,
};

static void gpio_stm32f1_init_one(gpio_stm32f1_port_t port)
{
  gpio_stm32f1_cfg_unpacked_t *cfg = &gpio_stm32f1_occ[port];

  cfg->port = port;
  occ_pack_flush_init(&cfg->head, &gpio_stm32f1_occ_pack_flush_ops);
}

void gpio_stm32f1_init(void)
{
  gpio_stm32f1_init_one(GPIOA_PORT);
  gpio_stm32f1_init_one(GPIOB_PORT);
  gpio_stm32f1_init_one(GPIOC_PORT);
  gpio_stm32f1_init_one(GPIOD_PORT);
  gpio_stm32f1_init_one(GPIOE_PORT);
}

#endif /* defined(STM32F103) */
