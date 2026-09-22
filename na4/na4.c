/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* na4.c - a simple tool to encode/decode base77 data                        */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* this code makes use of base77 with 6.203125 bits per character to         */
/* encode and decode chunks of 32 byte binary data into 42 ASCII characters  */
/*                                                                           */
/* if the -s option is enabled SHA256 is used to verify the encoded contents */
/* please pass an integer (N) to -s and feed N bytes to stdin for suffix MAC */
/*                                                                           */
/* please note that the contents are not encrypted but the SHA256 signature  */
/* may be used to check if the data has been tampered with or not            */
/*                                                                           */
/* by default the tool generates somewhat smaller amount of data compared    */
/* to base64 (for data sizes >= 32 bytes) but more importantly it also       */
/* allocates the bits wisely to squeeze in 4-bit CRC support in each frame   */
/*                                                                           */
/* the idea is to make a blend of efficiency and robustness with the main    */
/* tradeoff that the base77 slice and glue code is a tiny bit math heavy     */
/*                                                                           */
/* the character set is the same as the 1654.c base54 and base16 combined    */
/* but extended to be case sensitive and with the '*' character added:       */
/*                                                                           */
/* 0123456789                                                                */
/* ABCDEFGHIJKLMNOPQRSTUVWXYZ                                                */
/* abcdefghijklmnopqrstuvwxyz                                                */
/* (),-.<>@[]^_{}*                                                           */
/*                                                                           */
/* About the file format:                                                    */
/* each frame is made up of a header and data                                */
/* there are three types of headers in sizes from 0 to 2 characters          */
/* after the header follows N characters of encoded data                     */
/* one encoded frame contains a maximum of 32 encoded bytes as 42 characters */
/* the data stream is made up of one or several encoded frames in a sequence */
/*                                                                           */
/* there are two kinds of frames:                                            */
/* - regular frames                                                          */
/* - tail frames                                                             */
/*                                                                           */
/* a regular frame is also known as a 0 character header and is the common   */
/* case used to encode chunks of 32 bytes into 42 character frames. the 0    */
/* character header may be detected by checking the value of the first       */
/* character of the frame. only bottom 64 characters are used to encode the  */
/* first character of a regular frame. when decoding, if the first character */
/* turns out to be within the bottom 64 character set then it needs to be    */
/* further processed to extract the encoded data                             */
/*                                                                           */
/* the top characters in the character set are used to flag that a frame     */
/* should be treated as a tail frame. base77 characters are mostly used      */
/* freely to encode data in a regular frame, however the first character of  */
/* a frame is a special case and the following calculation shows that there  */
/* should be enough bits available by encoding 41 characters as base77 but   */
/* encode the first character in a frame as base64:                          */
/*                                                                           */
/* (6.203125 * 41) + 6 = 260.328125 which covers 256 bits of data and 4 CRC  */
/*                                                                           */
/* TAIL1 frames simply use a prefix character to determine the tail size     */
/* such as 1-byte frames, 2-byte frames, 31-byte frames and TAIL2 format     */
/* TAIL2 uses an additional character to also encode the remaining sizes     */
/*                                                                           */
/* if enabled when encoding, SHA256 data is stored in two TAIL2 frames       */
/*                                                                           */
/* When encoding the internal process looks like this:                       */
/* bin in -> BigInt(mul256_div77) -> reverse -> [tail] encoding -> char out  */
/*                                                                           */
/* Decoding is pretty much the reverse of encoding:                          */
/* char in -> [tail] decoding -> reverse -> BigInt(mul77_div256) -> bin out  */
/*                                                                           */
/* TODO:                                                                     */
/* - Clean up the decoder and the encoder                                    */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

char nananana[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz(),-.<>@[]^_{}*";

#define INPUT_BUFSIZE 32 /* 32 bytes input per frame maximum */
#define PROCESS_BUFSIZE 33 /* 32 bytes input + 4 bits CRC */
#define OUTPUT_BUFSIZE 42 /* 42 character output per frame maximum */

/* sha256 enablement */
void *sha256_enabled;
static int sha256_is_enabled(void)
{
  return !!sha256_enabled;
}

/* encoding math broken out from BigInt prototype (thank you Gemini) */
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

/* Precomputed table for CRC-4 ITU (x^4 + x + 1), poly = 0x3, MSB-first */
static const uint8_t crc4_table[16] = {
  0x0, 0x3, 0x6, 0x5, 0xC, 0xF, 0xA, 0x9,
  0xB, 0x8, 0xD, 0xE, 0x7, 0x4, 0x1, 0x2
};

/* CRC-4 implementation (thank you Gemini) */
static uint8_t crc4_itu(const uint8_t *data, size_t len)
{
  uint8_t crc = 0x0;
  int i;

  for (i = 0; i < len; i++) {
    crc = crc4_table[crc ^ (data[i] >> 4)];
    crc = crc4_table[crc ^ (data[i] & 0x0F)];
  }

  return crc;
}

#define BITS(n) ((n) / 8)
#define REGULAR(n) (n)                      /* 0 character header */
#define TAIL1(b, c) ((b) + (c ? 1 : 0) + 1) /* 1 character header */
#define TAIL2(b, c) ((b) + (c ? 1 : 0) + 2) /* 2 character header */

/* map between source encoding length and bits used to generate chars out */
static uint16_t frame_size[] = {
  [BITS(8)] = TAIL1(2, 0),      /* 12.41 bits DATA + 4 bits CRC */
  [BITS(16)] = TAIL1(3, 4),     /* 18.61 bits DATA + 4 bits CRC */
  [BITS(24)] = TAIL2(4, 4),     /* 24.81 bits DATA + 4 bits CRC */
  [BITS(32)] = TAIL2(6, 4),     /* 37.21 bits DATA + 4 bits CRC */
  [BITS(40)] = TAIL2(7, 4),     /* 43.42 bits DATA + 4 bits CRC */
  [BITS(48)] = TAIL2(8, 4),     /* 49.62 bits DATA + 4 bits CRC */
  [BITS(56)] = TAIL2(10, 0),    /* 62.03 bits DATA + 4 bits CRC */
  [BITS(64)] = TAIL2(11, 0),    /* 68.23 bits DATA + 4 bits CRC */
  [BITS(72)] = TAIL2(12, 4),    /* 74.43 bits DATA + 4 bits CRC */
  [BITS(80)] = TAIL2(13, 4),    /* 80.64 bits DATA + 4 bits CRC */
  [BITS(88)] = TAIL2(15, 0),    /* 93.04 bits DATA + 4 bits CRC */
  [BITS(96)] = TAIL2(16, 4),    /* 99.25 bits DATA + 4 bits CRC */
  [BITS(104)] = TAIL2(17, 4),   /* 105.42 bits DATA + 4 bits CRC */
  [BITS(112)] = TAIL2(19, 0),   /* 117.85 bits DATA + 4 bits CRC */
  [BITS(120)] = TAIL2(20, 0),   /* 124.06 bits DATA + 4 bits CRC */
  [BITS(128)] = TAIL2(21, 4),   /* 130.26 bits DATA + 4 bits CRC */
  [BITS(136)] = TAIL2(22, 4),   /* 136.46 bits DATA + 4 bits CRC */
  [BITS(144)] = TAIL2(24, 0),   /* 148.87 bits DATA + 4 bits CRC */
  [BITS(152)] = TAIL2(25, 4),   /* 155.07 bits DATA + 4 bits CRC */
  [BITS(160)] = TAIL2(26, 4),   /* 161.28 bits DATA + 4 bits CRC */
  [BITS(168)] = TAIL2(28, 0),   /* 173.69 bits DATA + 4 bits CRC */
  [BITS(176)] = TAIL2(29, 4),   /* 179.89 bits DATA + 4 bits CRC */
  [BITS(184)] = TAIL2(30, 4),   /* 186.09 bits DATA + 4 bits CRC */
  [BITS(192)] = TAIL2(31, 4),   /* 192.29 bits DATA + 4 bits CRC */
  [BITS(200)] = TAIL2(33, 0),   /* 204.70 bits DATA + 4 bits CRC */
  [BITS(208)] = TAIL2(34, 4),   /* 210.91 bits DATA + 4 bits CRC */
  [BITS(216)] = TAIL2(35, 4),   /* 217.11 bits DATA + 4 bits CRC */
  [BITS(224)] = TAIL2(37, 0),   /* 229.52 bits DATA + 4 bits CRC */
  [BITS(232)] = TAIL2(38, 4),   /* 235.72 bits DATA + 4 bits CRC */
  [BITS(240)] = TAIL2(39, 4),   /* 241.92 bits DATA + 4 bits CRC */
  [BITS(248)] = TAIL1(40, 4),   /* 248.13 bits DATA + 4 bits CRC */
  [BITS(256)] = REGULAR(42),    /* 260.33 bits DATA + 4 bits CRC */
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

static void reverse_data(uint8_t *dst, uint8_t *src,
                         int src_len, int expected_size)
{
  int i, s;

  /* when encoding, the data from the bigint processing comes out aligned */
  /* to src[0] which means that any leading zeroes are at the src[41] side. */
  /* however as part of the frame encoding used by this software we need to */
  /* encode the first character as base64 which means if the rest of the */
  /* logic operates on src[0] as first character value then we need to */
  /* reverse the bytes so any unused bits ends up towards src[0]. */
  /* without this the src[0] data will not always allow base64 encoding */

  s = expected_size - 1;

  for (i = 0; i < src_len; i++) {
    if ((s - i) >= 0) {
      dst[i] = src[s - i];
    }
  }
}

static int encode_frame_custom(uint8_t *buf, int len, char custom_tail)
{
  uint8_t num[PROCESS_BUFSIZE] = {};
  uint8_t rem[OUTPUT_BUFSIZE] = {};
  uint8_t rev[OUTPUT_BUFSIZE] = {};
  int i, s, hdr_size;
  uint8_t r = crc4_itu(buf, len);

  for (i = 0; i < len; i++) {
    num[i] = (buf[i] >> 4) | (r << 4);
    r = buf[i] & 0x0f;
  }
  num[i] = r;

  bigint_process(rem, OUTPUT_BUFSIZE, num, PROCESS_BUFSIZE,
                 bigint_mul256_div77);

  /* FIXME: this ad-hoc header size calculation needs to be improved */
  hdr_size = 2;
  if ((len == 1) || (len == 2) || (len == 31)) {
    hdr_size = 1;
  } else if (len == 32) {
    hdr_size = 0;
  }

  reverse_data(rev, rem, OUTPUT_BUFSIZE, frame_size[len] - hdr_size);

  if (custom_tail) {
    output_tail(custom_tail, encode_char(len - 3), rev, frame_size[len]);
  } else if (len == 32) {
  /* any frame with less than 32 bytes input data needs tail encoding */
    /* the first char must be less than 64 when encoding full frames */
    s = check_bottom_64(encode_char(rev[0]));
    if (s != 1) {
      fprintf(stderr, "error: unable to encode the first character\n");
      return -1;
    }
    output_tail(0, 0, rev, 42);
  } else if (len == 1) {
    output_tail(encode_char_top_64(12), 0, rev, frame_size[len]);
  } else if (len == 2) {
    output_tail(encode_char_top_64(11), 0, rev, frame_size[len]);
  } else if (len == 31) {
    output_tail(encode_char_top_64(10), 0, rev, frame_size[len]);
  } else {
    output_tail(encode_char_top_64(9), encode_char(len - 3),
                rev, frame_size[len]);
  }
  return len;
}

/* decoding math, the reverse of the encoding processing */
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
static int decode_frame_custom(uint8_t *dst, int dst_len,
                               uint8_t *buf, int len,
                               int *dst_bytes,
                               int (*handle_custom_tail)(int, uint8_t *, int))
{
  uint8_t num[PROCESS_BUFSIZE] = {};
  uint8_t chars[OUTPUT_BUFSIZE] = {};
  uint8_t rev[OUTPUT_BUFSIZE] = {};
  int i, n, s;
  int offs, expected_size;
  int output_length;
  int is_custom_tail = 0;

  /* convert ASCII encoded data to integers */
  for (i = 0; i < len; i++) {
    n = decode_char(buf[i]);
    if (n < 0) {
      fprintf(stderr, "error: unable to decode ASCII data for char %d\n", i);
      return -1;
    }
    chars[i] = n;
  }

  s = check_bottom_64(encode_char(chars[0]));
  if (s < 0) {
    fprintf(stderr, "error: unable to decode the first character\n");
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
      fprintf(stderr, "error: tail length char out of range (%d)\n", chars[1]);
      return -1;
    }
    offs = 2;
    expected_size = chars[1] + 3 + 1;
  } else if (encode_char(chars[0]) == encode_char_top_64(8)) {
    if (chars[1] != (16 - 3)) {
      fprintf(stderr, "error: sha256 tail length char mismatch\n");
      return -1;
    }
    is_custom_tail = encode_char_top_64(8);
    offs = 2;
    expected_size = chars[1] + 3 + 1;
  } else if (encode_char(chars[0]) == encode_char_top_64(7)) {
    if (chars[1] != (16 - 3)) {
      fprintf(stderr, "error: sha256 tail length char mismatch\n");
      return -1;
    }
    is_custom_tail = encode_char_top_64(7);
    offs = 2;
    expected_size = chars[1] + 3 + 1;
  } else {
    fprintf(stderr, "error: unsupported tail character\n");
    return -1;
  }

  output_length = frame_size[expected_size - 1];

  reverse_data(rev, &chars[offs], OUTPUT_BUFSIZE - offs,
               frame_size[expected_size - 1] - offs);

  bigint_process(num, PROCESS_BUFSIZE, rev, OUTPUT_BUFSIZE,
                 bigint_mul77_div256);

  {
    uint8_t r = num[0] & 0x0f;
    uint8_t tmp;

    for (i = 1; i < (expected_size - 1); i++) {
      tmp = num[i];
      num[i] = (tmp >> 4) | (r << 4);
      r = tmp & 0x0f;
    }
    num[i] |= (r << 4);

    r = crc4_itu(&num[1], expected_size - 1);
    if (r != (num[0] >> 4)) {
      fprintf(stderr, "error: crc mismatch (%d, %d)\n", r, num[0] >> 4);
      return -1;
    }
  }

  n = MIN(dst_len, expected_size - 1);

  if (is_custom_tail) {
    if (handle_custom_tail) {
      handle_custom_tail(is_custom_tail, &num[1], n);
    }
    
    if (dst_bytes)
      *dst_bytes = 0;
  } else {
    memcpy(dst, &num[1], n);

    if (dst_bytes)
      *dst_bytes = n;
  }

  return output_length; /* number of source bytes processed */
}

/* SHA256 implementation (thanks Gemini) */

#define SHA256_DIGEST_SIZE 32

typedef struct {
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];
} SHA256_CTX;

#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define ROTR(x, n)   (((x) >> (n)) | ((x) << (32 - (n))))
#define SIGMA0(x)    (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x)    (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x)    (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define sigma1(x)    (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  uint32_t w[64];
  int i;

  for (i = 0; i < 16; i++) {
    w[i] = ((uint32_t)block[i * 4] << 24) |
           ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) |
           ((uint32_t)block[i * 4 + 3]);
  }
  for (i = 16; i < 64; i++) {
    w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16];
  }

  for (i = 0; i < 64; i++) {
    uint32_t t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + w[i];
    uint32_t t2 = SIGMA0(a) + MAJ(a, b, c);
    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }

  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha256_init(SHA256_CTX *ctx)
{
  ctx->count = 0;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len)
{
  size_t index = (size_t)((ctx->count >> 3) % 64);
  ctx->count += ((uint64_t)len << 3);

  size_t part_len = 64 - index;
  size_t i = 0;

  if (len >= part_len) {
    memcpy(&ctx->buffer[index], data, part_len);
    sha256_transform(ctx->state, ctx->buffer);
    for (i = part_len; i + 64 <= len; i += 64) {
        sha256_transform(ctx->state, &data[i]);
    }
    index = 0;
  }
  memcpy(&ctx->buffer[index], &data[i], len - i);
}

static void sha256_final(uint8_t digest[SHA256_DIGEST_SIZE], SHA256_CTX *ctx)
{
  static const uint8_t pad[64] = { 0x80 };
  uint8_t bits[8];

  for (int i = 0; i < 8; i++) {
    bits[i] = (uint8_t)((ctx->count >> ((7 - i) * 8)) & 0xFF);
  }

  size_t index = (size_t)((ctx->count >> 3) % 64);
  size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
  sha256_update(ctx, pad, pad_len);
  sha256_update(ctx, bits, 8);

  for (int i = 0; i < 8; i++) {
    digest[i * 4]     = (uint8_t)((ctx->state[i] >> 24) & 0xFF);
    digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFF);
    digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xFF);
    digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFF);
  }
}

SHA256_CTX sha256_global_ctx;

int sha256_derived_key_bytes = 0;
uint8_t sha256_derived_key[32];

int sha256_stored_sum_bytes = 0;
uint8_t sha256_stored_sum[32];

static void sha256_enable(void)
{
  sha256_enabled = &sha256_global_ctx;
  sha256_init(sha256_enabled);
}

/* Simple Suffix-MAC with SHA-256 */
/* when encoding, store the sha256 sum */
static int store_sha256(SHA256_CTX *sha256)
{
  uint8_t sha256_res[32];

  /* add the SHA256 of the secret key after the data payload */
  if (sha256) {
    if (sha256_derived_key_bytes) {
      sha256_update(sha256, sha256_derived_key, sha256_derived_key_bytes);
    }
    sha256_final(sha256_res, sha256);
  }

  /* store key as two custom tail frames (7 after 8) */
  encode_frame_custom(&sha256_res[0], 16, encode_char_top_64(8));
  encode_frame_custom(&sha256_res[16], 16, encode_char_top_64(7));
  return 0;
}

/* when decoding, compare with the stored sum */
static int compare_sha256(SHA256_CTX *sha256)
{
  uint8_t sha256_res[32];

  /* add the SHA256 of the secret key after the data payload */
  if (sha256) {
    if (sha256_derived_key_bytes) {
      sha256_update(sha256, sha256_derived_key, sha256_derived_key_bytes);
    }
    sha256_final(sha256_res, sha256);
  }

  if (sha256_stored_sum_bytes == 0) {
    fprintf(stderr, "error: sha256 sum not present in parsed data\n");
    return -1;
  }

  if (memcmp(sha256_res, sha256_stored_sum, 32) != 0) {
    fprintf(stderr, "error: sha256 sum mismatch\n");
    return -1;
  }

  //fprintf(stderr, "sha256 sum correct\n");
  return 0;
}

static int process_frame_sha256(SHA256_CTX *sha256, uint8_t *buf, int len)
{
  if (sha256) {
   sha256_update(sha256, buf, len);
  }

  return len;
}

static int encode_frame_sha256(SHA256_CTX *sha256, uint8_t *buf, int len)
{
  if (sha256) {
   sha256_update(sha256, buf, len);
  }

  return encode_frame_custom(buf, len, 0);
}

static int decode_custom_tail(int tail_type, uint8_t *buf, int len)
{
  if (tail_type == encode_char_top_64(8)) {
    if (sha256_stored_sum_bytes == 0) {
      memcpy(&sha256_stored_sum[0], buf, 16);
      //fprintf(stderr, "loading sum from tail type 8\n");
      sha256_stored_sum_bytes = 16;
    }
  }
  if (tail_type == encode_char_top_64(7)) {
    if (sha256_stored_sum_bytes == 16) {
      memcpy(&sha256_stored_sum[16], buf, 16);
      //fprintf(stderr, "loading sum from tail type 7\n");
      sha256_stored_sum_bytes = 32;
    }
  }
  return 0;
}

static int decode_frame_sha256(SHA256_CTX *sha256, uint8_t *buf, int len)
{
  uint8_t frame_out[INPUT_BUFSIZE];
  int bytes_out = 0;
  int res = 0;

  res = decode_frame_custom(frame_out, INPUT_BUFSIZE, buf, len,
			    &bytes_out, decode_custom_tail);
  if (res < 0) {
    return res;
  }

  if ((res > 0) && (bytes_out > 0)) {
    if (sha256) {
      sha256_update(sha256, frame_out, bytes_out);
    }
    fwrite(frame_out, bytes_out, 1, stdout);
  }
  return res;
}

static uint8_t buf[MAX(INPUT_BUFSIZE, OUTPUT_BUFSIZE)];

static int stdin_fread_sha256(int bufsize,
                              int (*f)(SHA256_CTX *, uint8_t *, int),
			      int num_bufs,
                              SHA256_CTX *sha256,
                              int (*c)(SHA256_CTX *))
{
  int total_bytes = 0;
  int cnt;
  int n, m;

  do {
    memset(buf, 0, bufsize);
    cnt = 0;

  read_again:
    /* read one byte at a time to fill up to bufsize */
    do {
      n = fread(&buf[cnt], 1, 1, stdin);
      if (n) {
        cnt++;
      }
    } while (n && (cnt < bufsize));

    m = 0;
    if (cnt > 0) {
      if (f)  {
        m = f(sha256, buf, cnt);
        if (m < 0) {
          return -1;
        }
      }
    }
    if (f && (m < cnt)) {
      memmove(&buf[0], &buf[m], bufsize - m);
      total_bytes += m;
      cnt -= m;
      goto read_again;
    }

    total_bytes += cnt;
    if (num_bufs == 1) {
      break;
    }
  } while (n > 0);

  if (c && sha256) {
    if (c(sha256) < 0) {
      return -1;
    }
  }

  return total_bytes;
}

int main(int argc, char **argv)
{
  int decode_enabled = 0;
  int sha256_secret_bytes = -1;
  int i = 1;

  while(1) {
    if (argc >= (i + 1)) {
      if (strcmp(argv[i], "--help") == 0) {
        fprintf(stdout, "%s: a simple Base77 encoder/decoder\n", argv[0]);
        return 0;
      } else if (strcmp(argv[i], "-s") == 0) {
        if ((argc >= (i + 2))) {
          if (sscanf(argv[i + 1], "%u", &sha256_secret_bytes) == 1) {
            if (!sha256_is_enabled()) {
              sha256_enable();
            }
            i += 2;
          }
        }
        if (sha256_secret_bytes == -1) {
          fprintf(stderr, "%s: unable to parse -s argument\n", argv[0]);
          return 1;
        }
        continue;
      } else if (strcmp(argv[i], "-d") == 0) {
        decode_enabled = 1;
        i++;
        continue;
      }
    }
    break;
  }

  if (sha256_secret_bytes >= 0) {
    if (sha256_secret_bytes == 0) {
      fprintf(stderr, "warning: SHA256 signature mode enabled "
                      "with zero secret (aka naive mode)\n");
    } else {
      SHA256_CTX derived_key_ctx;
      int key_bytes;

      sha256_init(&derived_key_ctx);
      key_bytes = stdin_fread_sha256(sha256_secret_bytes,
                                     process_frame_sha256, 1,
                                     &derived_key_ctx, NULL);

      if (key_bytes != sha256_secret_bytes) {
        fprintf(stderr, "error: unable to read secret (%d, %d)\n",
                key_bytes, sha256_secret_bytes);
        return 1;
      }
      
      sha256_final(sha256_derived_key, &derived_key_ctx);
      sha256_derived_key_bytes = 32;
    }
  }
  
  if (decode_enabled) {
    return stdin_fread_sha256(OUTPUT_BUFSIZE, decode_frame_sha256, 0,
                              sha256_enabled, compare_sha256) != 0;
  } else {
    return stdin_fread_sha256(INPUT_BUFSIZE, encode_frame_sha256, 0,
                              sha256_enabled, store_sha256) != 0;
  }
}
