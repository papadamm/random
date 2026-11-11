/* place the _SEGGER_RTT symbol with the RTT data structure in the lowest */
/* part of System RAM by using the .data section and making sure the object */
/* file is being put as early as possible in link order */

#define SEGGER_RTT_SECTION ".data"

static inline int __strlen(const char *str)
{
  int k;

  for (k = 0; str[k] != '\0'; k++) {
  }

  return k;
}

static void *__memcpy(void *dst, const void *src, int n)
{
  char *dst8 = dst;
  const char *src8 = src;
  int k;

  for (k = 0; k < n; k++) {
    dst8[k] = src8[k];
  }

  return dst;
}

// this makes the _SEGGER_RTT symbol contain "XXXxxx xxxXXX"
extern int rtt_init_cnt;
static inline char *strcpy(char *dst, const char *src)
{
  if (rtt_init_cnt ^= 0x01) {
    __memcpy(dst, "xxxXXX", 6);
  } else {
    __memcpy(dst, "XXXxxx", 6);
  }
  return dst;
}

#define strlen(x) rtt_strlen(x)

#include "rtt-pristine/SEGGER_RTT.c"

int rtt_init_cnt = 0;
