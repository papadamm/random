/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* na4.c - a simple tool to Base77 encode/decode data                        */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

char nananana[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz(),-.<>@[]^_{}*";

#define INPUT_BUFSIZE 32 /* 32 bytes input per frame maximum */
#define PROCESS_BUFSIZE 33 /* 32 bytes input + 4 bits CRC */
#define OUTPUT_BUFSIZE 42 /* 42 character output per frame maximum */

// Divides a BigInt in-place by 'divisor' and returns the remainder (%).
// Little-endian: limbs[0] is LS, limbs[len-1] is MS.
static uint8_t bigint_mul256_div77(uint8_t *limbs, int len)
{
  uint8_t remainder = 0;
  uint16_t current;
  uint8_t divisor = 77;
  int i;

  // Process from most-significant limb down to least-significant
  for (i = (len - 1); i >= 0; i--) {

    // Base is 256 (0x100) [multiply by 256 and divide by divisor]
    current = (remainder << 8) | limbs[i];
    limbs[i] = (uint8_t)(current / divisor);
    remainder = current % divisor;
  }

  return remainder;
}

/* divide limbs (len) with divisor and store result in rem */
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

/* generate an ASCII character using the predefined character set */
static char encode_char(uint8_t value)
{
  return nananana[value];
}

/* generate an ASCII character in top 64 range to encode tail frames */
static char encode_char_top_64(uint8_t offs)
{
  return nananana[64 + offs];
}

/* check that the ASCII character is in the bottom 64 range */
static int check_bottom_64(char ch)
{
  const char *str = &nananana[0];
  char *found;

  found = strchr(str, ch);
  if (!found) {
    return -1;
  }

  return !((found - str) >= 64);
}

static uint8_t calculate_crc(uint8_t *buf, int len)
{
  uint32_t cnt = 0;
  uint8_t data_in;
  int i;

  for (i = 0; i < len; i++) {
    data_in = buf[i];

    /* highly unoptimized hamming weight (replace with one processor opcode) */
    while (data_in) {
      if (data_in & 0x01) {
        cnt++;
      }
      data_in >>= 1;
    }
  }

  return cnt & 0x0f;
}

#define BITS(n) ((n) / 8)
#define TAIL1(b, c) ((b) + (c ? 1 : 0) + 1) /* 1 character header */
#define TAIL2(b, c) ((b) + (c ? 1 : 0) + 2) /* 2 character header */

/* map between source encoding length and bits used to generate chars out */
/* FIXME: the +1 isn't supposed to be there */
static uint16_t frame_size[] = {
  [BITS(8)] = TAIL1(2+1, 0),    /* 12.41 bits DATA + 4 bits CRC */
  [BITS(16)] = TAIL1(3, 4),     /* 18.61 bits DATA + 4 bits CRC */
  [BITS(24)] = TAIL2(4, 4),     /* 24.81 bits DATA + 4 bits CRC */
  [BITS(32)] = TAIL2(6, 4),     /* 37.21 bits DATA + 4 bits CRC */
  [BITS(40)] = TAIL2(7, 4),     /* 43.42 bits DATA + 4 bits CRC */
  [BITS(48)] = TAIL2(8, 4),     /* 49.62 bits DATA + 4 bits CRC */
  [BITS(56)] = TAIL2(10+1, 0),  /* 62.03 bits DATA + 4 bits CRC */
  [BITS(64)] = TAIL2(11+1, 0),  /* 68.23 bits DATA + 4 bits CRC */
  [BITS(72)] = TAIL2(12, 4),    /* 74.43 bits DATA + 4 bits CRC */
  [BITS(80)] = TAIL2(13, 4),    /* 80.64 bits DATA + 4 bits CRC */
  [BITS(88)] = TAIL2(15+1, 0),  /* 93.04 bits DATA + 4 bits CRC */
  [BITS(96)] = TAIL2(16, 4),    /* 99.25 bits DATA + 4 bits CRC */
  [BITS(104)] = TAIL2(17, 4),   /* 105.42 bits DATA + 4 bits CRC */
  [BITS(112)] = TAIL2(19+1, 0), /* 117.85 bits DATA + 4 bits CRC */
  [BITS(120)] = TAIL2(20+1, 0), /* 124.06 bits DATA + 4 bits CRC */
  [BITS(128)] = TAIL2(21, 4),   /* 130.26 bits DATA + 4 bits CRC */
  [BITS(136)] = TAIL2(22, 4),   /* 136.46 bits DATA + 4 bits CRC */
  [BITS(144)] = TAIL2(24+1, 0), /* 148.87 bits DATA + 4 bits CRC */
  [BITS(152)] = TAIL2(25, 4),   /* 155.07 bits DATA + 4 bits CRC */
  [BITS(160)] = TAIL2(26, 4),   /* 161.28 bits DATA + 4 bits CRC */
  [BITS(168)] = TAIL2(28+1, 0), /* 173.69 bits DATA + 4 bits CRC */
  [BITS(176)] = TAIL2(29, 4),   /* 179.89 bits DATA + 4 bits CRC */
  [BITS(184)] = TAIL2(30, 4),   /* 186.09 bits DATA + 4 bits CRC */
  [BITS(192)] = TAIL2(31, 4),   /* 192.29 bits DATA + 4 bits CRC */
  [BITS(200)] = TAIL2(33+1, 0), /* 204.70 bits DATA + 4 bits CRC */
  [BITS(208)] = TAIL2(34, 4),   /* 210.91 bits DATA + 4 bits CRC */
  [BITS(216)] = TAIL2(35, 4),   /* 217.11 bits DATA + 4 bits CRC */
  [BITS(224)] = TAIL2(37+1, 0), /* 229.52 bits DATA + 4 bits CRC */
  [BITS(232)] = TAIL2(38, 4),   /* 235.72 bits DATA + 4 bits CRC */
  [BITS(240)] = TAIL2(39, 4),   /* 241.92 bits DATA + 4 bits CRC */
  [BITS(248)] = TAIL1(40, 4),   /* 248.13 bits DATA + 4 bits CRC */
};

static void output_tail(char tail1, char tail2, uint8_t *buf, int len)
{
  int adj = 0;
  int i;

  if (tail1) {
    printf("%c", tail1);
    adj++;
  }

  if (tail2) {
    printf("%c", tail2);
    adj++;
  }

  /* output encoded data on stdout */
  for (i = 0; i < (len - adj); i++) {
    printf("%c", encode_char(buf[i]));
  }
}

static int encode_frame(uint8_t *buf, int len)
{
  uint8_t num[PROCESS_BUFSIZE] = {};
  uint8_t rem[OUTPUT_BUFSIZE] = {};
  int i, s;
  uint8_t r = calculate_crc(buf, len);

  for (i = 0; i < len; i++) {
    num[i] = (buf[i] >> 4) | (r << 4);
    r = buf[i] & 0x0f;
  }
  num[i] = r << 4;

  bigint_process(rem, OUTPUT_BUFSIZE, num, PROCESS_BUFSIZE,
                 bigint_mul256_div77);

  /* any frame with less than 32 bytes input data needs tail encoding */
  if (len == 32) {
    /* the first char must be less than 64 when encoding full frames */
    s = check_bottom_64(encode_char(rem[0]));
    if (s != 1) {
      fprintf(stderr, "unable to encode the first character\n");
      return -1;
    }
    output_tail(0, 0, rem, 42);
  } else if (len == 1) {
    output_tail(encode_char_top_64(12), 0, rem, frame_size[len]);
  } else if (len == 2) {
    output_tail(encode_char_top_64(11), 0, rem, frame_size[len]);
  } else if (len == 31) {
    output_tail(encode_char_top_64(10), 0, rem, frame_size[len]);
  } else {
    output_tail(encode_char_top_64(9), encode_char(len - 3),
                rem, frame_size[len]);
  }
  return 0;
}

static uint8_t bigint_mul77_div256(uint8_t *limbs, int len)
{
  uint8_t carry = 0;
  uint16_t current;
  int i;

  // Process from most-significant limb down to least-significant
  for (i = (len - 1); i >= 0; i--) {

    // Base is 77 [multiply by 77 and divide by 256]
    current = (carry * 77) + limbs[i];
    limbs[i] = current >> 8;
    carry = current & 0xff;
  }

  return carry;
}

static int decode_char(int ch)
{
  const char *str = &nananana[0];
  char *found;

  found = strchr(str, ch);
  if (found) {
    return found - str;
  }

  return -1;
}

/* decode incoming ASCII characters, generate binary data */
static int decode_frame(uint8_t *buf, int len)
{
  uint8_t num[PROCESS_BUFSIZE] = {};
  uint8_t chars[OUTPUT_BUFSIZE] = {};
  int i, n, s;
  int offs, expected_size;

  /* convert ASCII encoded data to integers */
  for (i = 0; i < len; i++) {
    n = decode_char(buf[i]);
    if (n < 0) {
      fprintf(stderr, "unable to decode ASCII data for character %d\n", i);
      return -1;
    }
    chars[i] = n;
  }

  s = check_bottom_64(encode_char(chars[0]));
  if (s < 0) {
    fprintf(stderr, "unable to decode the first character\n");
    return -1;
  } else if (s == 1) { /* full frame, expect 42 ASCII characters */
    offs = 0;
    expected_size = 33;
  } else if (encode_char(chars[0]) == encode_char_top_64(12)) {
    /* one byte of data */
    offs = 1;
    expected_size = 2;
  } else if (encode_char(chars[0]) == encode_char_top_64(11)) {
    /* two bytes of data */
    offs = 1;
    expected_size = 3;
  } else if (encode_char(chars[0]) == encode_char_top_64(10)) {
    /* 31 bytes of data */
    offs = 1;
    expected_size = 32;
  } else if (encode_char(chars[0]) == encode_char_top_64(9)) {
    /* N bytes of data */
    if (chars[1] > 27) {
      fprintf(stderr, "tail length character out of range (%d)\n", chars[1]);
      return -1;
    }
    offs = 2;
    expected_size = chars[1] + 3 + 1;
  } else {
    fprintf(stderr, "unsupported tail character\n");
    return -1;
  }

  bigint_process(num, PROCESS_BUFSIZE, &chars[offs], len + offs,
                 bigint_mul77_div256);

  {
    uint8_t r = num[0] & 0x0f;
    uint8_t tmp;

    for (i = 1; i < expected_size; i++) {
      tmp = num[i];
      num[i] = (tmp >> 4) | (r << 4);
      r = tmp & 0x0f;
    }
    num[i] = r;

    if (calculate_crc(&num[1], expected_size - 1) != (num[0] >> 4)) {
      fprintf(stderr, "crc mismatch (%d)\n", expected_size);
      return -1;
    }
  }

  fwrite(&num[1], expected_size - 1, 1, stdout);
  return 0;
}

#define MAX(x,y) ((x) > (y) ? (x) : (y))
static uint8_t buf[MAX(INPUT_BUFSIZE, OUTPUT_BUFSIZE)];

static int stdin_fread(int bufsize, int (*f)(uint8_t *limbs, int len))
{
  int cnt;
  int n;

  do {
    memset(buf, 0, bufsize);
    cnt = 0;
    
    /* read one byte at a time to fill up to bufsize */
    do {
      n = fread(&buf[cnt], 1, 1, stdin);
      if (n) {
        cnt++;
      }
    } while (n && (cnt < bufsize));

    if (cnt > 0) {
      if (f(buf, cnt) < 0) {
        return -1;
      }
    }
  } while (n > 0);

  return 0;
}

int main(int argc, char **argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "--help") == 0) {
      fprintf(stdout, "%s: a simple Base77 encoder/decoder\n", argv[0]);
      return 0;
    }
    if (strcmp(argv[1], "-d") == 0) {
      return stdin_fread(OUTPUT_BUFSIZE, decode_frame) != 0;
    }
  }
  return stdin_fread(INPUT_BUFSIZE, encode_frame) != 0;
}
