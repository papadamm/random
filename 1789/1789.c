/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* 1789.c - encode/decode 8 <-> 19 bytes of data (1 + 53^7 + 53^7 + 1 data)  */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */

#ifndef __STANDALONE__
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define __BUILD_ENCODER__
#define __BUILD_DECODER__
#endif

char latin_charset[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";
char special_chars[] = "$*#";
char whitespace_chars[] = " \t\r\n";

#define INPUT_BUFSIZE 8 /* INPUT is up to 8 bytes of data per frame */
#define OUTPUT_BUFSIZE 19 /* OUTPUT consists of 19 characters */

/* the encode process is as follows: */
/* 8 bytes input data is split into 2 x 4 byte units */
/* each 4 byte unit is base53 encoded into 7 bytes */
/* the encoded data is output as 19 characters in this specific format: */
/* $17*71# where $*# are fixed, 1 are parameters and 7 is base53 data */
/* also the 1 parameters are the first character of the base53 charset */

#define INPUT_UNIT_BUFSIZE 5 /* 32-bit data */
#define PROCESS_UNIT_BUFSIZE 6
#define OUTPUT_UNIT_BUFSIZE 7

typedef enum { err_encode_1st_char, err_decode_ascii,
               err_decode_1st_char, err_decode_char,
               err_buffer_failure, err_invalid_parameter } err_t;

struct context_1789 {
  char charset_a[53];
  char charset_b[53];
  int has_encoded_data;
  int is_decoding;
  int line_wrap;
  int line_wrap_cnt;
  int (*read)(struct context_1789 *, uint8_t *, int);
  int (*write)(struct context_1789 *, uint8_t *, int);
  void (*flush)(struct context_1789 *);
  void (*err)(struct context_1789 *, err_t);
};

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define ERROR(c, m) c->err(c, m)

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

static int check_bottom_50(char *charset, char ch)
{
  const char *str = charset;
  char *found;

  found = __strchr(str, ch);
  if (!found) {
    return -1;
  }

  return !((found - str) >= 50);
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

static char encode_char(char *charset, uint8_t value)
{
  return charset[value];
}

static int write_data(struct context_1789 *ctx, uint8_t *buf, int bytes)
{
  int m;

  if (ctx->line_wrap && !ctx->is_decoding) {
    for (m = 0; m < bytes; m++) {
      ctx->write(ctx, &buf[m], 1);
      ctx->line_wrap_cnt++;

      if (ctx->line_wrap_cnt == ctx->line_wrap) {
        ctx->write(ctx, (uint8_t *)"\n", 1);
	ctx->line_wrap_cnt = 0;
      }
    }
  } else {
    ctx->write(ctx, buf, bytes);
  }

  return bytes;
}

#ifdef __BUILD_ENCODER__

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

static void output_data(struct context_1789 *ctx, char *charset, char tail,
                        uint8_t *buf, int len, int reverse)
{
  int i;

  if (reverse) {
    for (i = len - 1; i >= 0; i--) {
      uint8_t ch = encode_char(charset, buf[i]);
      write_data(ctx, &ch, 1);
    }

    if (tail) {
      write_data(ctx, (void *)&tail, 1);
    }
  } else {
    if (tail) {
      write_data(ctx, (void *)&tail, 1);
    }

    for (i = 0; i < len; i++) {
      uint8_t ch = encode_char(charset, buf[i]);
      write_data(ctx, &ch, 1);
    }
  }

  ctx->flush(ctx);
}

static int encode_unit_frame(void *handle, char *charset,
                             uint8_t *buf, int len, int reverse)
{
  struct context_1789 *ctx = handle;
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
    s = check_bottom_50(charset, encode_char(charset, rev[0]));
    if (s != 1) {
      ERROR(ctx, err_encode_1st_char);
      return -1;
    }
    output_data(ctx, charset, 0, rev, 7, reverse);
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

    output_data(ctx, charset, encode_char(charset, 52), rev, 6, reverse);
  }
  return len;
}

static int encode_frame(void *handle, uint8_t *buf, int len)
{
  struct context_1789 *ctx = handle;
  uint8_t zeroed_buffer[INPUT_UNIT_BUFSIZE];
  int status;
  int m, p;
  int n = 0;

  /* allow encoding a first single 19-byte frame of zero length data */
  if (ctx->has_encoded_data && (len == 0)) {
    return 0;
  }

  write_data(ctx, (void *)&special_chars[0], 1);
  write_data(ctx, (void *)&ctx->charset_a[0], 1);

  for (m = 0; m < 2; m++) {
    memset(zeroed_buffer, 0, INPUT_UNIT_BUFSIZE);
    p = MIN(len, 4);

    if (p) {
      memcpy(&zeroed_buffer[0], buf, p);
    }

    status = encode_unit_frame(handle,
                               m ? &ctx->charset_b[0] : &ctx->charset_a[0],
                               zeroed_buffer, p, m == 1);
    buf += p;
    len -= p;

    if (status < 0) {
      return status;
    } else {
      n += status;
    }
    if (m) {
      write_data(ctx, (void *)&ctx->charset_b[0], 1);
    }
    write_data(ctx, (void *)&special_chars[m + 1], 1);
  }

  if (n) {
    ctx->has_encoded_data = 1;
  }

  return n;
}

#endif /* __BUILD_ENCODER__ */
#ifdef __BUILD_DECODER__

static int decode_char(char *charset, char ch)
{
  const char *str = charset;
  char *found;

  found = __strchr(str, ch);
  if (found) {
    return found - str;
  }

  return -1;
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

static int decode_unit_frame(void *handle,
                             char *charset,
                             uint8_t *dst, int dst_len,
                             uint8_t *buf, int len,
                             int *dst_bytes,
                             int reverse)
{
  struct context_1789 *ctx = handle;
  uint8_t num[PROCESS_UNIT_BUFSIZE] = {};
  uint8_t chars[OUTPUT_UNIT_BUFSIZE] = {};
  uint8_t rev[OUTPUT_UNIT_BUFSIZE] = {};
  int i, n, s;

  /* convert ASCII encoded data to integers */
  for (i = 0; i < len; i++) {
    if (reverse) {
      chars[i] = decode_char(charset, buf[len - (i + 1)]);
    } else {
      chars[i] = decode_char(charset, buf[i]);
    }
    if (chars[i] < 0) {
      ERROR(ctx, err_decode_ascii);
      return -1;
    }
  }

  s = check_bottom_50(charset, encode_char(charset, chars[0]));
  if (s < 0) {
    ERROR(ctx, err_decode_1st_char);
    return -1;
  } else if (s == 1) { /* full frame containing 5 bytes data */
    reverse_data(rev, &chars[0], OUTPUT_UNIT_BUFSIZE, 7);
    n = 5;
  } else { /* partial frame containing 0 -> 4 bytes */
    if (encode_char(charset, chars[0]) != encode_char(charset, 52)) {
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

static int decode_frame(void *handle, uint8_t *buf, int len)
{
  struct context_1789 *ctx = handle;
  uint8_t buf_lacking_space[OUTPUT_BUFSIZE + 1] = {};
  uint8_t frame_a[INPUT_UNIT_BUFSIZE] = {};
  uint8_t frame_b[INPUT_UNIT_BUFSIZE] = {};
  int bytes_out_a = 0;
  int bytes_out_b = 0;
  int res_a = 0;
  int res_b = 0;
  uint8_t *src;
  int m, n, c;

  if (len == 0)
    return 0;

  c = 0;
  /* skip through any white space characters when decoding */
  if (ctx->line_wrap) {
    n = 0;
    for (m = 0; m < len; m++) {
      if (n < OUTPUT_BUFSIZE) {
        if (__strchr(whitespace_chars, buf[m]) == 0) {
          buf_lacking_space[n++] = buf[m];
        } else {
          c++;
        }
      }
    }
    src = &buf_lacking_space[0];
  } else {
    src = buf;
  }

  /* check for "$" */
  if (src[0] != special_chars[0]) {
    ERROR(ctx, err_decode_1st_char);
    return -1;
  }

  if (src[1] != ctx->charset_a[0]) {
    ERROR(ctx, err_decode_char);
    return -1;
  }

  /* check for "*" */
  if (src[9] != special_chars[1]) {
    ERROR(ctx, err_decode_char);
    return -1;
  }

  if (src[17] != ctx->charset_b[0]) {
    ERROR(ctx, err_decode_char);
    return -1;
  }

  /* check for "#" */
  if (src[18] != special_chars[2]) {
    ERROR(ctx, err_decode_char);
    return -1;
  }

  res_a = decode_unit_frame(handle, &ctx->charset_a[0],
                            frame_a, sizeof(frame_a),
                            &src[2], OUTPUT_UNIT_BUFSIZE,
                            &bytes_out_a, 0);

  if (res_a < 0) {
    return res_a;
  }

  res_b = decode_unit_frame(handle, &ctx->charset_b[0],
                            frame_b, sizeof(frame_b),
                            &src[10], OUTPUT_UNIT_BUFSIZE,
                            &bytes_out_b, 1);

  if (res_b < 0) {
    return res_b;
  }

  write_data(ctx, frame_a, bytes_out_a);
  write_data(ctx, frame_b, bytes_out_b);
  ctx->flush(ctx);

  return res_a + res_b + 5 + c;
}

#endif /* __BUILD_DECODER__ */

/* when decoding use OUTPUT_BUFSIZE *2 for worst case white space scenario */
#define MAX_BUFSIZE MAX(INPUT_BUFSIZE, (OUTPUT_BUFSIZE * 2))

static int read_data(void *handle,
                     int bufsize,
                     int (*f)(void *, uint8_t *, int))
{
  struct context_1789 *ctx = handle;
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

#ifndef __STANDALONE__

int encode_or_decode(struct context_1789 *ctx)
{
  if (ctx->is_decoding) {
    return read_data(ctx, OUTPUT_BUFSIZE * 2, decode_frame) < 0;
  } else {
    return read_data(ctx, INPUT_BUFSIZE, encode_frame) < 0;
  }
}

#define ERR_MSG(n, str) [n] = str

char *err_msg[] = {
  ERR_MSG(err_encode_1st_char, "unable to encode the first subframe char"),
  ERR_MSG(err_decode_ascii, "unable to decode ASCII data"),
  ERR_MSG(err_decode_1st_char, "unable to decode the subframe first char"),
  ERR_MSG(err_decode_char, "unable to decode partial frame char"),
  ERR_MSG(err_buffer_failure, "buffer failure"),
  ERR_MSG(err_invalid_parameter, "invalid parameter"),
};

static int read_1789(struct context_1789 *ctx, uint8_t *buf, int bytes)
{
  return fread(buf, 1, bytes, stdin);
}

static int write_1789(struct context_1789 *ctx, uint8_t *buf, int bytes)
{
  return fwrite(buf, bytes, 1, stdout);
}

static void flush_1789(struct context_1789 *ctx)
{
  fflush(stdout);
}

static void err_1789(struct context_1789 *ctx, err_t err)
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
  "Parameters:\n"
  "  -a character    First Base53 character for first subframe\n"
  "  -b character    First Base53 character for last subframe\n"
  "  -c cols         Line wrap output every <cols> chars, default off\n"
  "\n"
  "Informational:\n"
  "  -h, --help      Display this help message and exit\n"
  "\n"
  "  Encode:\n"
  "    cat payload.bin | %s -a '$' -b '#' > encoded.1789\n"
  "\n"
  "  Decode:\n"
  "    cat encoded.1789 | %s -d -a '$' -b '#' > out.bin\n"
  "\n"
  "Exit Status:\n"
  "  0   Success\n"
  "  1   Corrupted framing\n",
  prog_name, prog_name, prog_name
  );
}

int main(int argc, char **argv)
{
  struct context_1789 ctx = {};
  struct context_1789 *cp = &ctx;
  int i = 1;

  ctx.err = err_1789;
  ctx.read = read_1789;
  ctx.write = write_1789;
  ctx.flush = flush_1789;

  memcpy(&ctx.charset_a[1], latin_charset, sizeof(latin_charset));
  memcpy(&ctx.charset_b[1], latin_charset, sizeof(latin_charset));

  while(1) {
    if (argc >= (i + 1)) {
      if (strcmp(argv[i], "--help") == 0) {
        print_help(argv[0]);
        return 0;
      }
      if (strcmp(argv[i], "-d") == 0) {
        ctx.is_decoding = 1;
        i++;
        continue;
      }
      if (strcmp(argv[i], "-a") == 0) {
        if ((argc >= (i + 2))) {
          if (sscanf(argv[i + 1], "%c", &ctx.charset_a[0]) == 1) {
            i += 2;
            continue;
          }
        }
      }
      if (strcmp(argv[i], "-b") == 0) {
        if ((argc >= (i + 2))) {
          if (sscanf(argv[i + 1], "%c", &ctx.charset_b[0]) == 1) {
            i += 2;
            continue;
          }
        }
      }
      if (strcmp(argv[i], "-c") == 0) {
        if ((argc >= (i + 2))) {
          if (sscanf(argv[i + 1], "%u", &ctx.line_wrap) == 1) {
            i += 2;
            continue;
          }
        }
      }
    }
    break;
  }

  /* the charset parameters -a and -b are mandatory */
  if ((ctx.charset_a[0] == '\0') || (ctx.charset_b[0] == '\0')) {
    ERROR(cp, err_invalid_parameter);
    return 1;
  }

  /* the charset parameters must not be part of the regular charset */
  if (__strchr(&ctx.charset_a[1], ctx.charset_a[0]) != 0) {
    ERROR(cp, err_invalid_parameter);
    return 1;
  }

  if (__strchr(&ctx.charset_b[1], ctx.charset_b[0]) != 0) {
    ERROR(cp, err_invalid_parameter);
    return 1;
  }

  /* in case line wrap is on, check charset parameters against white space */
  if (ctx.line_wrap) {
    if (__strchr(&whitespace_chars[0], ctx.charset_a[0]) != 0) {
      ERROR(cp, err_invalid_parameter);
      return 1;
    }

    if (__strchr(&whitespace_chars[0], ctx.charset_b[0]) != 0) {
      ERROR(cp, err_invalid_parameter);
      return 1;
    }
  }

  return encode_or_decode(cp);
}

#endif /* __STANDALONE__ */
