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

static int encode_frame(uint8_t *buf, int len)
{
  uint8_t num[PROCESS_BUFSIZE] = {};
  uint8_t rem[OUTPUT_BUFSIZE] = {};
  int i, n, s;
  uint8_t r = calculate_crc(buf, len);

  for (i = 0; i < (len + 1); i++) {
    num[i] = ((buf[i] & 0xf) << 4) | r;
    r = buf[i] >> 4;
  }

  n = bigint_process(rem, OUTPUT_BUFSIZE, num, PROCESS_BUFSIZE,
		     bigint_mul256_div77);

  if (len == 32) {
    if (n != 42) {
      fprintf(stderr, "unexpected length %d for a full frame\n", n);
      return -1;
    }
    /* the first char must be less than 64 when encoding full frames */
    if (n > 0) {
      s = check_bottom_64(encode_char(rem[0]));
      if (s != 1) {
        fprintf(stderr, "unable to encode the first character\n");
        return -1;
      }
    }
  } else {
    /* any frame encoding less than 32 bytes needs tail encoding */
    if (len == 1) {
      printf("%c", encode_char_top_64(12));
    } else if (len == 2) {
      printf("%c", encode_char_top_64(11));
    } else if (len == 31) {
      printf("%c", encode_char_top_64(10));
    } else {
      printf("%c", encode_char_top_64(9));
      printf("%c", encode_char(len));
    }
  }

  /* output encoded data on stdout */
  for (i = 0; i < n; i++) {
    printf("%c", encode_char(rem[i]));
  }

  return 0;
}

/* encode binary input stream to ASCII character output */
static int encode(void)
{
  uint8_t buf[INPUT_BUFSIZE];
  int cnt;
  int n;

  do {
    memset(buf, 0, INPUT_BUFSIZE);
    cnt = 0;

    /* read one byte at a time to fill up to INPUT_BUFSIZE */
    do {
      n = fread(&buf[cnt], 1, 1, stdin);
      if (n) {
	cnt++;
      }
    } while (n && (cnt < INPUT_BUFSIZE));

    if (cnt > 0) {
      if (encode_frame(buf, cnt) < 0) {
	return -1;
      }
    }
  } while (n > 0);

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
    expected_size = 31;
  } else if (encode_char(chars[0]) == encode_char_top_64(9)) {
    /* N bytes of data */
    offs = 2;
    expected_size = chars[1] + 1;
  } else {
    fprintf(stderr, "unsupported tail character\n");
    return -1;
  }

  n = bigint_process(num, PROCESS_BUFSIZE, &chars[offs], len + offs,
		     bigint_mul77_div256);

  if (n != expected_size) {
    fprintf(stderr, "outside expected range (%d, %d)\n", n, expected_size);
    return -1;
  }

  /* swizzle data back to [1] and keep crc in [0] */
  for (i = n; i >= 1; i--) {
    num[i] = ((num[i] & 0x0f) << 4) | (num[i - 1] >> 4);
  }

  if (calculate_crc(&num[1], n - 1) != (num[0] & 0x0f)) {
    fprintf(stderr, "crc mismatch when reconstructing %d bytes\n", n);
    return -1;
  }

  fwrite(&num[1], n - 1, 1, stdout);
  return 0;
}

/* decode by going backwards from OUTPUT_BUFSIZE to INPUT_BUFSIZE */
static int decode(void)
{
  uint8_t buf[OUTPUT_BUFSIZE];
  int cnt;
  int n;

  do {
    memset(buf, 0, OUTPUT_BUFSIZE);
    cnt = 0;
    
    /* read one byte at a time to fill up to OUTPUT_BUFSIZE */
    do {
      n = fread(&buf[cnt], 1, 1, stdin);
      if (n) {
        cnt++;
      }
    } while (n && (cnt < OUTPUT_BUFSIZE));
  
    if (cnt > 0) {
      if (decode_frame(buf, cnt) < 0) {
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
      return decode() != 0;
    }
  }
  return encode() != 0;
}
