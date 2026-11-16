/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* 1536.c - a simple tool to encode/decode 15 <-> 36 bytes of base53 data    */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

char charset_1536[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz#";
char char_airgap = ' '; /* white space */

#define INPUT_BUFSIZE 15 /* INPUT is up to 15 bytes of data per frame */
#define OUTPUT_BUFSIZE 21 /* OUTPUT consists of 15 + 21 = 36 characters */

/* the encode process is as follows: */
/* 15 bytes input data is split into 3 x 5 byte units */
/* each 5 byte unit is base53 encoded into fixed 7 bytes frames */
/* when outputting data 3 times repeat outputing 5 x air and 7 x base53 data */
/* for a total of 36 characters output */

#define INPUT_UNIT_BUFSIZE 5 /* internally process 1/3 at a time */
#define PROCESS_UNIT_BUFSIZE 6
#define OUTPUT_UNIT_BUFSIZE 7 /* 5 bytes input encodes to 7 characters */

typedef enum { err_encode_1st_char, err_decode_ascii,
               err_decode_1st_char, err_decode_char,
               err_buffer_failure, } err_t;

struct context_1536 {
  int has_encoded_data;
  int (*read)(struct context_1536 *, uint8_t *, int);
  int (*write)(struct context_1536 *, uint8_t *, int);
  void (*flush)(struct context_1536 *);
  void (*err)(struct context_1536 *, err_t);
};

#define ERROR(c, m) c->err(c, m)

static uint8_t bigint_mul256_div53(uint8_t *limbs, int len)
{
  uint8_t remainder = 0;
  uint16_t current;
  uint8_t divisor = 53;
  int i;

  /* Process from most-significant limb down to least-significant */
  for (i = (len - 1); i >= 0; i--) {

    /* Base is 256 (0x100) [multiply by 256 and divide by divisor] */
    current = (remainder << 8) | limbs[i];
    limbs[i] = (uint8_t)(current / divisor);
    remainder = current % divisor;
  }

  return remainder;
}

static uint8_t bigint_mul53_div256(uint8_t *limbs, int len)
{
  uint8_t carry = 0;
  uint16_t current;
  int i;

  /* Process from most-significant limb down to least-significant */
  for (i = (len - 1); i >= 0; i--) {

    /* Base is 53 [multiply by 53 and divide by 256] */
    current = (carry * 53) + limbs[i];
    limbs[i] = current >> 8;
    carry = current & 0xff;
  }

  return carry;
}

static int bigint_process(uint8_t *r, int r_len,
                          uint8_t *limbs, int len,
                          uint8_t (*f)(uint8_t *limbs, int len))
{
  int r_cnt = 0;
  int i;

  while (r_len > r_cnt) {
    /* check if the limbs still contain non-zero data */
    for (i = len - 1; i >= 0; i--) {
      if (limbs[i] != 0) {
        break;
      }
    }

    /* all zero data, nothing left to do, return */
    if (i < 0) {
      return r_cnt;
    }

    /* process once, modify limbs */
    r[r_cnt++] = f(limbs, len);
  }

  if (r_cnt == r_len) {
    return r_cnt;
  }

  return -1; /* does not fit in r[] */
}

static char encode_char(uint8_t value)
{
  return charset_1536[value];
}

static char *__strchr(const char *s, int c)
{
  int i;

  for (i = 0; s[i] != '\0'; i++) {
    if (s[i] == c) {
      return (void *)&s[i];
    }
  }

  return NULL;
}

static int decode_char(int ch)
{
  const char *str = &charset_1536[0];
  char *found;

  found = __strchr(str, ch);
  if (found) {
    return found - str;
  }

  return -1;
}

static int check_bottom_50(char ch)
{
  const char *str = &charset_1536[0];
  char *found;

  found = __strchr(str, ch);
  if (!found) {
    return -1;
  }

  return !((found - str) >= 50);
}

static void output_tail(struct context_1536 *ctx, char airgap,
                        char tail, uint8_t *buf, int len)
{
  int i;

  if (airgap) {
    for (i = 0; i < 5; i++) {
      ctx->write(ctx, (void *)&airgap, 1);
    }
  }

  if (tail) {
    ctx->write(ctx, (void *)&tail, 1);
  }

  /* output encoded data */
  for (i = 0; i < len; i++) {
    uint8_t ch = encode_char(buf[i]);
    ctx->write(ctx, &ch, 1);
  }

  ctx->flush(ctx);
}

static void reverse_data(uint8_t *dst, uint8_t *src,
                         int src_len, int expected_size)
{
  int i, s;

  /* when encoding, the data from the bigint processing comes out aligned */
  /* to src[0] which means that any leading zeroes are at the src[N] side. */
  /* however as part of the frame encoding used by this software we need to */
  /* encode the first character as baseN which means if the rest of the */
  /* logic operates on src[0] as first character value then we need to */
  /* reverse the bytes so any unused bits ends up towards src[0]. */
  /* without this the src[0] data will not always allow baseN encoding */

  s = expected_size - 1;

  for (i = 0; i < src_len; i++) {
    if ((s - i) >= 0) {
      dst[i] = src[s - i];
    }
  }
}

static int encode_unit_frame(void *handle, uint8_t *buf, int len)
{
  struct context_1536 *ctx = handle;
  uint8_t num[PROCESS_UNIT_BUFSIZE] = {};
  uint8_t rem[OUTPUT_UNIT_BUFSIZE] = {};
  uint8_t rev[OUTPUT_UNIT_BUFSIZE] = {};
  int i, s;

  for (i = 0; i < len; i++) {
    num[i] = buf[i];
  }

  bigint_process(rem, OUTPUT_UNIT_BUFSIZE, num, PROCESS_UNIT_BUFSIZE,
                 bigint_mul256_div53);

  if (len == 5) { /* encode full frame */
    reverse_data(rev, rem, OUTPUT_UNIT_BUFSIZE, 7);

    /* the first char must be less than 50 when encoding full frames */
    s = check_bottom_50(encode_char(rev[0]));
    if (s != 1) {
      ERROR(ctx, err_encode_1st_char);
      return -1;
    }
    output_tail(ctx, char_airgap, 0, rev, 7);
  } else {
    reverse_data(rev, rem, OUTPUT_UNIT_BUFSIZE, 6);

    /* include encode size data */
    switch (len) {
    case 0: rev[0] |= (0 * 10); break;
    case 1: rev[0] |= (1 * 10); break;
    case 2: rev[0] |= (2 * 10); break;
    case 3: rev[0] |= (3 * 10); break;
    case 4: rev[0] |= (4 * 10); break;
    }

    output_tail(ctx, char_airgap, encode_char(52), rev, 6);
  }
  return len;
}

static int decode_unit_frame(void *handle,
                             uint8_t *dst, int dst_len,
                             uint8_t *buf, int len,
                             int *dst_bytes)
{
  struct context_1536 *ctx = handle;
  uint8_t num[PROCESS_UNIT_BUFSIZE] = {};
  uint8_t chars[OUTPUT_UNIT_BUFSIZE] = {};
  uint8_t rev[OUTPUT_UNIT_BUFSIZE] = {};
  int i, n, s;

  /* convert ASCII encoded data to integers */
  for (i = 0; i < len; i++) {
    n = decode_char(buf[i]);
    if (n < 0) {
      ERROR(ctx, err_decode_ascii);
      return -1;
    }
    chars[i] = n;
  }

  s = check_bottom_50(encode_char(chars[0]));
  if (s < 0) {
    ERROR(ctx, err_decode_1st_char);
    return -1;
  } else if (s == 1) { /* full frame containing 5 bytes data */
    reverse_data(rev, &chars[0], OUTPUT_UNIT_BUFSIZE, 7);
    n = 5;
  } else { /* partial frame containing 0 -> 4 bytes */
    if (encode_char(chars[0]) != encode_char(52)) {
      ERROR(ctx, err_decode_1st_char);
      return -1;
    }

    n = -1;
    switch (chars[1] / 10) {
    case 0: n = 0; break;
    case 1: n = 1; break;
    case 2: n = 2; break;
    case 3: n = 3; break;
    case 4: n = 4; break;
    default:
      ERROR(ctx, err_decode_char);
      return -1;
    }
    /* strip away size encoding data */
    chars[1] = chars[1] % 10;
    reverse_data(rev, &chars[1], OUTPUT_UNIT_BUFSIZE, 6);
  }

  bigint_process(num, PROCESS_UNIT_BUFSIZE,
                 rev, OUTPUT_UNIT_BUFSIZE,
                 bigint_mul53_div256);

  memcpy(dst, &num[0], n);

  if (dst_bytes) {
    *dst_bytes = n;
  }

  return 7; /* number of source bytes processed */
}

static int encode_frame(void *handle, uint8_t *buf, int len)
{
  struct context_1536 *ctx = handle;
  uint8_t zeroed_buffer[INPUT_UNIT_BUFSIZE];
  int status;
  int m;
  int n = 0;

  /* allow encoding a first single 15-byte frame of zero length data */
  if (ctx->has_encoded_data && (len == 0)) {
    return 0;
  }

  for (m = 0; m < 3; m++) {
    if (len >= INPUT_UNIT_BUFSIZE) {
      status = encode_unit_frame(handle, buf, INPUT_UNIT_BUFSIZE);
      buf += INPUT_UNIT_BUFSIZE;
      len -= INPUT_UNIT_BUFSIZE;
    } else {
      memset(zeroed_buffer, 0, INPUT_UNIT_BUFSIZE);
      if (len) {
        memcpy(zeroed_buffer, buf, len);
      }
      status = encode_unit_frame(handle, zeroed_buffer, len);
      buf += len;
      len -= len;
    }

    if (status < 0) {
      return status;
    } else {
      n += status;
    }
  }

  if (n) {
    ctx->has_encoded_data = 1;
  }

  return n;
}

static int decode_frame(void *handle, uint8_t *buf, int len)
{
  struct context_1536 *ctx = handle;
  uint8_t frame_out[INPUT_UNIT_BUFSIZE];
  int bytes_out = 0;
  int res = 0;

  if (len == 0)
    return 0;

  /* skip through airgap characters when decoding */
  if (buf[0] == char_airgap) {
    return 1;
  }

  res = decode_unit_frame(handle, frame_out, INPUT_UNIT_BUFSIZE, buf, len,
                          &bytes_out);

  if (res < 0) {
    return res;
  }

  if ((res == 0) || (bytes_out <= 0)) {
    return res;
  }

  ctx->write(ctx, frame_out, bytes_out);
  ctx->flush(ctx);
  return res;
}

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MAX_BUFSIZE MAX(INPUT_BUFSIZE, OUTPUT_BUFSIZE)

static int read_data(void *handle,
                     int bufsize,
                     int (*f)(void *, uint8_t *, int))
{
  struct context_1536 *ctx = handle;
  uint8_t buf[MAX_BUFSIZE];
  int total_bytes = 0;
  int cnt;
  int n, m;

  if (bufsize > MAX_BUFSIZE) {
    ERROR(ctx, err_buffer_failure);
    return -1;
  }

  do {
    memset(buf, 0, bufsize);
    cnt = 0;

  read_again:
    /* read one byte at a time to fill up to bufsize */
    do {
      n = ctx->read(ctx, &buf[cnt], 1);
      if (n) {
        cnt++;
      }
    } while (n && (cnt < bufsize));

    m = 0;
    if (f)  {
      m = f(handle, buf, cnt);
      if (m < 0) {
        return -1;
      }
    }
    if (f && (m < cnt)) {
      memmove(&buf[0], &buf[m], bufsize - m);
      total_bytes += m;
      cnt -= m;
      goto read_again;
    }

    total_bytes += cnt;
  } while (n > 0);

  return total_bytes;
}

int encode_or_decode(struct context_1536 *ctx, int decode_enabled)
{
  if (decode_enabled) {
    return read_data(ctx, OUTPUT_UNIT_BUFSIZE, decode_frame) < 0;
  } else {
    return read_data(ctx, INPUT_BUFSIZE, encode_frame) < 0;
  }
}

#define ERR_MSG(n, str) [n] = str

char *err_msg[] = {
  ERR_MSG(err_encode_1st_char, "unable to encode the first char"),
  ERR_MSG(err_decode_ascii, "unable to decode ASCII data"),
  ERR_MSG(err_decode_1st_char, "unable to decode the first char"),
  ERR_MSG(err_decode_char, "unable to decode partial frame char"),
  ERR_MSG(err_buffer_failure, "buffer failure"),
};

static int read_1536(struct context_1536 *ctx, uint8_t *buf, int bytes)
{
  return fread(buf, 1, bytes, stdin);
}

static int write_1536(struct context_1536 *ctx, uint8_t *buf, int bytes)
{
  return fwrite(buf, bytes, 1, stdout);
}

static void flush_1536(struct context_1536 *ctx)
{
  fflush(stdout);
}

static void err_1536(struct context_1536 *ctx, err_t err)
{
  fprintf(stderr, "error: %s\n", err_msg[err]);
}

static void print_help(const char *prog_name)
{
  fprintf(stdout,
  "Usage: %s [OPTIONS]\n"
  "\n"
  "A portable, dependency-free UNIX streaming filter for framing data\n"
  "over standard pipes.\n"
  "\n"
  "Operational Modes:\n"
  "  -d              Decode mode (default: encode)\n"
  "\n"
  "Informational:\n"
  "  -h, --help      Display this help message and exit\n"
  "\n"
  "  Encode:\n"
  "    cat payload.bin | %s > encoded.1536\n"
  "\n"
  "  Decode:\n"
  "    cat encoded.1536 | %s -d > out.bin\n"
  "\n"
  "Exit Status:\n"
  "  0   Success\n"
  "  1   Corrupted framing\n",
  prog_name, prog_name, prog_name
  );
}

int main(int argc, char **argv)
{
  struct context_1536 ctx = {};
  int decode_enabled = 0;
  int i = 1;

  ctx.err = err_1536;
  ctx.read = read_1536;
  ctx.write = write_1536;
  ctx.flush = flush_1536;

  while(1) {
    if (argc >= (i + 1)) {
      if (strcmp(argv[i], "--help") == 0) {
        print_help(argv[0]);
        return 0;
      }
      if (strcmp(argv[i], "-d") == 0) {
        decode_enabled = 1;
        i++;
        continue;
      }
    }
    break;
  }

  return encode_or_decode(&ctx, decode_enabled);
}
