/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* na4.c - a simple tool to encode/decode base77 data                        */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* this code makes use of base77 with 6.203125 bits per character to         */
/* encode and decode chunks of 32 byte binary data into 42 ASCII characters  */
/*                                                                           */
/* by default the tool will encode. use the -d option to switch to decode.   */
/* the data gets coded binary <-> ASCII with an embedded CRC4 checksum.      */
/* any amount data (including 0-byte) will pass through in the default mode. */
/*                                                                           */
/* if the -s option is enabled SHA256 is used to verify the encoded contents */
/* please pass an integer (N) to -s and feed N bytes to stdin for suffix MAC */
/* note that the encoded contents will pass through regardless SHA256 match. */
/* feeding data without signature (or empty) to na4 -d -s N will cause error */
/*                                                                           */
/* if the -e option is enabled AES256 is used to encrypt the data. for this  */
/* to work the -s option needs to be enabled as well. when encryption is on  */
/* the tool will perform an early check to see if the secrets are matching.  */
/* unless matching there will be no output of the contents. also in this     */
/* mode it is possible to pass through empty data. to improve privacy the    */
/* size of the encoded data will vary randomly when encryption is enabled.   */
/*                                                                           */
/* by default the tool generates somewhat smaller amount of data compared    */
/* to base64 (for data sizes >= 32 bytes) but more importantly it also       */
/* allocates the bits wisely to squeeze in 4-bit CRC support in each frame   */
/*                                                                           */
/* the idea is to make a blend of efficiency and robustness with the main    */
/* tradeoff that the base77 slice and glue code is a tiny bit math heavy     */
/* however compared to SHA256 and AES256 the default mode is quite light     */
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
/* depending on if signature is added and if encryption is enabled:          */
/* + "-s" appends signature data in two TAIL2 frames at the end              */
/* + "-e" adds a constant sized prefix made from one unencrypted TAIL2 frame */
/* + "-e" also adds another variable sized prefix to the encrypted stream    */
/*                                                                           */
/* When encoding the internal process looks like this:                       */
/* bin in -> BigInt(mul256_div77) -> reverse -> [tail] encoding -> char out  */
/*                                                                           */
/* Decoding is pretty much the reverse of encoding:                          */
/* char in -> [tail] decoding -> reverse -> BigInt(mul77_div256) -> bin out  */
/*                                                                           */
/* TODO:                                                                     */
/* - Clean up the decoder and the encoder (especially the function names)    */
/* - Clean up header size calculation and avoid duplicating logic            */
/* - Tail frame detection logic needs to be straightened out                 */
/* - stdin_fread() is not exactly easy to read. stdin_fread_secret() is.     */
/* - Global variables for moshio and crypto headers are not exactly clean    */
/* - Switch from suffix MAC to HMAC-SHA256                                   */
/* - Add test cases for the various command line options                     */
/* - Add help text                                                           */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define KDF_ITERATIONS 100000

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

char nananana[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz(),-.<>@[]^_{}*";

#define INPUT_BUFSIZE 32 /* 32 bytes input per frame maximum */
#define PROCESS_BUFSIZE 33 /* 32 bytes input + 4 bits CRC */
#define OUTPUT_BUFSIZE 42 /* 42 character output per frame maximum */

typedef struct {
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];
} SHA256_CTX;

struct na4_context {
  SHA256_CTX sha256_ctx;
  SHA256_CTX saved_sha256_ctx;
  int aes256_enabled;
  int sha256_enabled;
  int crypto_header_parsed;
  int crypto_moshio_required;
  uint8_t crypto_moshio_data;
  int sha256_derived_key_bytes;
  uint8_t sha256_derived_key[32];
  int sha256_decoded_signature_bytes;
  uint8_t sha256_decoded_signature[32];

};

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

  fflush(stdout);
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

static int encode_frame_custom(void *handle,
                               uint8_t *buf, int len,
                               char custom_tail)
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
static int decode_frame_custom(void *handle,
                               uint8_t *dst, int dst_len,
                               uint8_t *buf, int len,
                               int *dst_bytes,
                               int (*handle_custom_tail)
                                   (void *, int, uint8_t *, int))
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
  } else if (encode_char(chars[0]) == encode_char_top_64(6)) {
    if (chars[1] != (20 - 3)) {
      fprintf(stderr, "error: crypto tail length char mismatch\n");
      return -1;
    }
    is_custom_tail = encode_char_top_64(6);
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
      if (handle_custom_tail(handle, is_custom_tail, &num[1], n) < 0) {
	return -1;
      }
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

/* AES encoder implementation (thanks Gemini) */

#define AES256_ROUNDS 14
#define AES256_EXP_KEY_SIZE (16 * (AES256_ROUNDS + 1)) /* 240 bytes */

typedef struct {
  uint8_t round_keys[AES256_EXP_KEY_SIZE];
} aes256_ctx_t;

aes256_ctx_t na4_aes256_ctx;

static void aes256_set_key(aes256_ctx_t *ctx, const uint8_t key[32]);

/* AES-CTR implementation (thanks Gemini) */

typedef struct {
  uint8_t block[16];   /* bytes 0..11: nonce, bytes 12..15: big-endian cnt */
  uint32_t counter;    /* integer tracker */
} na4_ctr_state_t;

na4_ctr_state_t na4_ctr_state;
static void ctr_init(na4_ctr_state_t *state, const uint8_t nonce[12]);
static void aes_ctr_process_frame(uint8_t *data, size_t len,
                                  na4_ctr_state_t *ctr,
                                  const aes256_ctx_t *aes_ctx);

/* Simple Suffix-MAC with SHA-256 */
/* when encoding, store the sha256 sum */
static int store_signature(void *handle, int total_bytes)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;
  uint8_t sha256_res[32];

  /* no need to store SHA256 when "-s" is missing */
  if (!ctx->sha256_enabled) {
    return 0;
  }

  if ((total_bytes == 0) && ctx->sha256_enabled && !ctx->aes256_enabled) {
    fprintf(stderr, "warning: cannot safely sign 0-byte stream "
            "without -e (salt); omitting signature\n");
    return 0;
  }

  /* add the SHA256 of the secret key after the data payload */
  if (sha256) {
    if (ctx->sha256_derived_key_bytes) {
      sha256_update(sha256, ctx->sha256_derived_key,
                    ctx->sha256_derived_key_bytes);
    }
    sha256_final(sha256_res, sha256);
  }

  /* store key as two custom tail frames (7 after 8) */
  encode_frame_custom(handle, &sha256_res[0], 16, encode_char_top_64(8));
  encode_frame_custom(handle, &sha256_res[16], 16, encode_char_top_64(7));
  return 0;
}

/* when decoding, compare with the stored sum */
static int compare_signature(void *handle, int total_bytes)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;
  uint8_t sha256_res[32];

  /* no need to compare SHA256 when "-s" is missing */
  if (!ctx->sha256_enabled) {
    return 0;
  }

  /* add the SHA256 of the secret key after the data payload */
  if (sha256) {
    if (ctx->sha256_derived_key_bytes) {
      sha256_update(sha256, ctx->sha256_derived_key,
                    ctx->sha256_derived_key_bytes);
    }
    sha256_final(sha256_res, sha256);
  }

  if (ctx->sha256_decoded_signature_bytes != 32) {
    fprintf(stderr, "error: empty stream or missing signature\n");
    return -1;
  }

  if (memcmp(sha256_res, ctx->sha256_decoded_signature, 32) != 0) {
    fprintf(stderr, "error: signature mismatch\n");
    return -1;
  }
  return 0; /* signature correct */
}

static int finish_secret_plaintext(void *handle, int total_bytes)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;

  sha256_final(ctx->sha256_derived_key, sha256);
  ctx->sha256_derived_key_bytes = 32;

  /* initialize once more, this time for actual data processing */
  sha256_init(sha256);
  return 0;
}

static int crypto_init_salt(uint8_t *buf, int len);

static int encode_frame(void *handle, uint8_t *buf, int len)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;
  uint8_t moshio_frame[INPUT_BUFSIZE];
  int nr_moshio_bytes;
  int nr_data_bytes;
  int n;

  /* initial encrypted moshio frame, prevents disclosing encoded data size */
  if (ctx->crypto_moshio_required) {
    /* generate 32 bytes of random data, use 1-16 bytes as prefix */
    if (crypto_init_salt(&moshio_frame[0], INPUT_BUFSIZE) < 0) {
      return -1;
    }

    /* store first frame length at a position determined by the hash */
    nr_moshio_bytes = ctx->crypto_moshio_data & 0x0f;
    nr_data_bytes = MIN(INPUT_BUFSIZE - (nr_moshio_bytes + 1), len);
    moshio_frame[nr_moshio_bytes] = nr_data_bytes;
    nr_moshio_bytes++;
    memcpy(&moshio_frame[nr_moshio_bytes], buf, nr_data_bytes);

    n = nr_moshio_bytes + nr_data_bytes;

    if (ctx->aes256_enabled) {
      aes_ctr_process_frame(&moshio_frame[0], n,
			    &na4_ctr_state, &na4_aes256_ctx);
    }

    if (ctx->sha256_enabled) {
      sha256_update(sha256, &moshio_frame[0], n);
    }

    encode_frame_custom(handle, &moshio_frame[0], n, 0);

    ctx->crypto_moshio_required = 0;
    return nr_data_bytes; /* short */
  }

  /* regular frame path */
  if (len == 0)
    return 0;

  if (ctx->aes256_enabled) {
    aes_ctr_process_frame(buf, len, &na4_ctr_state, &na4_aes256_ctx);
  }

  if (ctx->sha256_enabled) {
   sha256_update(sha256, buf, len);
  }

  return encode_frame_custom(handle, buf, len, 0);
}

static int process_frame_decrypt_late(void *handle, uint8_t *buf, int len);

static int decode_custom_tail(void *handle, int tail_type,
                              uint8_t *buf, int len)
{
  struct na4_context *ctx = handle;

  if (tail_type == encode_char_top_64(8)) {
    if (ctx->sha256_decoded_signature_bytes == 0) {
      memcpy(&ctx->sha256_decoded_signature[0], buf, 16);
      ctx->sha256_decoded_signature_bytes = 16;
    }
  }
  if (tail_type == encode_char_top_64(7)) {
    if (ctx->sha256_decoded_signature_bytes == 16) {
      memcpy(&ctx->sha256_decoded_signature[16], buf, 16);
      ctx->sha256_decoded_signature_bytes = 32;
    }
  }
  if (tail_type == encode_char_top_64(6)) {
    return process_frame_decrypt_late(handle, buf, len);
  }
  return 0;
}

static int decode_frame(void *handle, uint8_t *buf, int len)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;
  uint8_t frame_out[INPUT_BUFSIZE];
  int bytes_out = 0;
  int res = 0;

  if (len == 0)
    return 0;

  res = decode_frame_custom(handle, frame_out, INPUT_BUFSIZE, buf, len,
			    &bytes_out, decode_custom_tail);
  if (res < 0) {
    return res;
  }

  if (ctx->aes256_enabled && !ctx->crypto_header_parsed) {
    fprintf(stderr, "error: stream is not encrypted, but -e was specified\n");
    return  -1;
  }

  if ((res == 0) || (bytes_out <= 0)) {
    return res;
  }

  if (ctx->sha256_enabled) {
    sha256_update(sha256, frame_out, bytes_out);
  }
  if (ctx->aes256_enabled) {
    aes_ctr_process_frame(frame_out, bytes_out,
                          &na4_ctr_state, &na4_aes256_ctx);
  }

  /* simply skip over initial encrypted moshio data */
  if (ctx->crypto_moshio_required) {
    int nr_moshio_bytes;
    int stored_len;
    int nr_data_bytes;

    /* determine number of moshio bytes to skip by the hash */
    nr_moshio_bytes = ctx->crypto_moshio_data & 0x0f;
    stored_len = frame_out[nr_moshio_bytes];
    nr_moshio_bytes++;
    nr_data_bytes = MIN(INPUT_BUFSIZE - nr_moshio_bytes, stored_len);

    ctx->crypto_moshio_required = 0;

    fwrite(&frame_out[nr_moshio_bytes], nr_data_bytes, 1, stdout);
    fflush(stdout);
    return res;
  }

  fwrite(frame_out, bytes_out, 1, stdout);
  fflush(stdout);
  return res;
}

#define MAX_BUFSIZE MAX(INPUT_BUFSIZE, OUTPUT_BUFSIZE)

static uint8_t buf[MAX_BUFSIZE];

static int stdin_fread(void *handle,
		       int (*c)(void *),
		       int bufsize,
		       int (*f)(void *, uint8_t *, int),
		       int (*e)(void *, int))
{
  int total_bytes = 0;
  int cnt;
  int n, m;

  if (c) {
    if (c(handle) < 0) {
      return -1;
    }
  }

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

  if (e) {
    if (e(handle, total_bytes) < 0) {
      return -1;
    }
  }

  return total_bytes;
}

static int stdin_fread_secret(void *handle,
                              int (*c)(void *),
                              int xfer_size,
                              int (*f)(void *, uint8_t *, int),
                              int (*e)(void *, int))
{
  int total_bytes = 0;
  uint8_t ch = 0;
  int n, m;

  if (c && (c(handle) < 0)) {
    return -1;
  }

  while (total_bytes < xfer_size) {
    n = fread(&ch, 1, 1, stdin);
    if (n > 0) {
      m = f(handle, &ch, 1);
      if (m < 0) {
        return -1;
      }
      total_bytes += m;
    }
  }

  if (e && (e(handle, total_bytes) < 0)) {
    return -1;
  }

  ch = 0;
  return total_bytes;
}

/* KDF key handling, thanks Gemini */

/* Header metadata packed into the first frame */
typedef struct {
  uint8_t salt[12];        /* Nonce / KDF Salt */
  uint8_t check_token[8];  /* Fast verification tag */
  uint8_t moshio[1];       /* Parameter used for encrypted salt */
} na4_crypto_hdr_t;

/* Holds derived active keys */
typedef struct {
  uint8_t aes_key[32];     /* AES-256-CTR key */
  uint8_t mac_key[32];     /* Stream integrity MAC key */
} na4_keys_t;

/*
 * Helper: PBKDF2-like iterated hashing using your SHA256 primitives.
 * Performs KDF_ITERATIONS rounds of SHA-256 over (salt || secret).
 */
static void kdf_extract_master_late(SHA256_CTX *base_ctx,
                                    uint8_t master_prk[32],
                                    const uint8_t *salt, size_t salt_len)
{
  SHA256_CTX ctx;
  uint32_t i;

  /* Round 1A: H(Secret) (done elsewhere before this) */
  /* Round 1B: H(Salt) */
  memcpy(&ctx, base_ctx, sizeof(SHA256_CTX));
  sha256_update(&ctx, salt, salt_len);
  sha256_final(master_prk, &ctx);

  memset(base_ctx, 0, sizeof(SHA256_CTX));

  /* --- Subsequent Rounds (2 .. KDF_ITERATIONS) --- */
  /* Pure iterated hash stretching: H(H(H(...))) */
  for (i = 1; i < KDF_ITERATIONS; i++) {
    sha256_init(&ctx);
    sha256_update(&ctx, master_prk, 32); /* Only hashing the 32-byte digest */
    sha256_final(master_prk, &ctx);
  }

  memset(&ctx, 0, sizeof(SHA256_CTX));
}

/*
 * Derives AES Key, MAC Key, and the 8-byte Check Token from the master key.
 */
static void kdf_expand_keys(na4_keys_t *keys,
                            uint8_t check_token[8],
                            uint8_t moshio[1],
                            const uint8_t master_prk[32])
{
  SHA256_CTX ctx;
  uint8_t h[32];

  /* 1. Derive AES-256 Key */
  sha256_init(&ctx);
  sha256_update(&ctx, master_prk, 32);
  sha256_update(&ctx, (const uint8_t *)"aes-enc", 7);
  sha256_final(keys->aes_key, &ctx);

  /* 2. Derive MAC Key */
  sha256_init(&ctx);
  sha256_update(&ctx, master_prk, 32);
  sha256_update(&ctx, (const uint8_t *)"mac-auth", 8);
  sha256_final(keys->mac_key, &ctx);

  /* 3. Derive Check Token (first 8 bytes of H(Master || "chk")) */
  sha256_init(&ctx);
  sha256_update(&ctx, master_prk, 32);
  sha256_update(&ctx, (const uint8_t *)"chk-tok", 7);
  sha256_final(h, &ctx);

  memcpy(check_token, h, 8);

  /* 4. Derive Moshio (encryped variable size salt) */
  sha256_init(&ctx);
  sha256_update(&ctx, master_prk, 32);
  sha256_update(&ctx, (const uint8_t *)"chk-moshio", 10);
  sha256_final(h, &ctx);

  memcpy(moshio, h, 1);

  /* Derive 12-byte CTR Nonce */
  sha256_init(&ctx);
  sha256_update(&ctx, master_prk, 32);
  sha256_update(&ctx, (const uint8_t *)"ctr-nonce", 9);
  sha256_final(h, &ctx);

  ctr_init(&na4_ctr_state, h);

  /* Clean up sensitive stack memory */
  memset(h, 0, sizeof(h));
}

static int crypto_init_salt(uint8_t *buf, int len)
{
  FILE *f;

  /* Generate N bytes of fresh random salt from CSPRNG */
  f = fopen("/dev/urandom", "rb");
  if (!f || fread(buf, 1, len, f) != len) {
    if (f) fclose(f);
      return -1;
  }
  fclose(f);
  return 0;
}

static int crypto_init_encoder_late(struct na4_context *ctx,
                                    na4_crypto_hdr_t *hdr, na4_keys_t *keys)
{
  uint8_t master_prk[32];

  /* 1. Generate 12 bytes of fresh random salt from CSPRNG (done) */
  /* 2. Compute iterated master key */
  kdf_extract_master_late(&ctx->sha256_ctx, master_prk, hdr->salt, 12);

  /* 3. Expand into several keys and check tokens */
  kdf_expand_keys(keys, hdr->check_token, hdr->moshio, master_prk);
  memset(master_prk, 0, sizeof(master_prk));

  /* Emit `hdr` (20 bytes: 12 bytes salt + 8 bytes token) as Frame 0 */
  encode_frame_custom((void *)ctx, (void *)hdr, 20, encode_char_top_64(6));
  return 0;
}

static int crypto_init_decoder(struct na4_context *ctx,
                               const na4_crypto_hdr_t *hdr, na4_keys_t *keys)
{
  uint8_t master_prk[32];
  uint8_t computed_token[8];
  uint8_t moshio[1];

  if (!ctx->aes256_enabled) {
    if (ctx->sha256_enabled) {
      fprintf(stderr, "error: encrypted stream requires -e\n");
    } else {
      fprintf(stderr,
              "error: stream is encrypted; secret required (-s / -e)\n");
    }
    return -1;
  }

  /* 1. Recompute the master key using the salt read from the file */
  kdf_extract_master_late(&ctx->saved_sha256_ctx, master_prk, hdr->salt, 12);

  /* 2. Expand keys and compute what the check token SHOULD be */
  kdf_expand_keys(keys, computed_token, moshio, master_prk);
  memset(master_prk, 0, sizeof(master_prk));

  /* 3. Constant-time comparison: did the password match? */
  /* (Use volatile or a loop to avoid timing optimization) */
  uint8_t diff = 0;
  for (int i = 0; i < 8; i++) {
    diff |= (computed_token[i] ^ hdr->check_token[i]);
  }

  if (diff != 0) {
    /* Wrong password! Clean up keys and fail immediately */
    memset(keys, 0, sizeof(na4_keys_t));
    fprintf(stderr, "error: incorrect password\n");
    return -1; 
  }

  /* next step is to parse a bit of encrypted salt */
  ctx->crypto_moshio_required = 1;
  ctx->crypto_moshio_data = moshio[0];

  /* Token matched! Ready to start decrypting frames straight to stdout */
  return 0;
}

static int init_secret(void *handle)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;

  /* initialize first time for processing the secret */
  sha256_init(sha256);
  return 0;
}

static int process_secret(void *handle, uint8_t *buf, int len)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;

  sha256_update(sha256, buf, len);
  memset(buf, 0, len); /* zero out the secret now when done */
  return len;
}

static int finish_secret_encrypt(void *handle, int total_bytes)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;
  na4_keys_t crypto_keys = {};
  na4_crypto_hdr_t crypto_hdr = {};

  if (crypto_init_salt(&crypto_hdr.salt[0], 12) < 0) {
    return -1;
  }

  crypto_init_encoder_late(ctx, &crypto_hdr, &crypto_keys);

  /* initialize once more, this time for actual data processing */
  sha256_init(sha256);

  /* save key for use later when data processing is finished */
  memcpy(ctx->sha256_derived_key, crypto_keys.mac_key,
         sizeof(crypto_keys.mac_key));
  ctx->sha256_derived_key_bytes = 32;

  aes256_set_key(&na4_aes256_ctx, &crypto_keys.aes_key[0]);

  /* next step is to output a bit of encrypted salt */
  ctx->crypto_moshio_required = 1;
  ctx->crypto_moshio_data = crypto_hdr.moshio[0];

  memset(&crypto_hdr, 0, sizeof(crypto_hdr));
  memset(&crypto_keys, 0, sizeof(crypto_keys));
  return 0;
}

static int finish_secret_decrypt(void *handle, int total_bytes)
{
  struct na4_context *ctx = handle;
  SHA256_CTX *sha256 = &ctx->sha256_ctx;

  /* save key context for use later when intializing the decoder */
  memcpy(&ctx->saved_sha256_ctx, sha256, sizeof(SHA256_CTX));

  /* initialize once more, this time for actual data processing */
  sha256_init(sha256);
  return 0;
}

static int process_frame_decrypt_late(void *handle, uint8_t *buf, int len)
{
  struct na4_context *ctx = handle;
  na4_keys_t crypto_keys = {};
  na4_crypto_hdr_t crypto_hdr = {};
  int ret = -1;

  if (!ctx->crypto_header_parsed) {
    memcpy(&crypto_hdr, buf, len);
    ret = crypto_init_decoder(ctx, &crypto_hdr, &crypto_keys);

    if (ret == 0) {
      /* save key for use later when data processing is finished */
      memcpy(ctx->sha256_derived_key, crypto_keys.mac_key,
             sizeof(crypto_keys.mac_key));
      ctx->sha256_derived_key_bytes = 32;

      aes256_set_key(&na4_aes256_ctx, &crypto_keys.aes_key[0]);
      ctx->crypto_header_parsed = 1;
    }
    memset(&crypto_hdr, 0, sizeof(crypto_hdr));
    memset(&crypto_keys, 0, sizeof(crypto_keys));
  }
  return ret;
}

static void ctr_init(na4_ctr_state_t *state, const uint8_t nonce[12])
{
  memcpy(state->block, nonce, 12);
  state->counter = 0;
  state->block[12] = 0;
  state->block[13] = 0;
  state->block[14] = 0;
  state->block[15] = 0;
}

static void ctr_increment(na4_ctr_state_t *state)
{
  state->counter++;
  /* Write 32-bit counter in Big-Endian format */
  state->block[12] = (uint8_t)(state->counter >> 24);
  state->block[13] = (uint8_t)(state->counter >> 16);
  state->block[14] = (uint8_t)(state->counter >> 8);
  state->block[15] = (uint8_t)(state->counter & 0xFF);
}

/* Forward S-Box table */
static const uint8_t aes_sbox[256] = {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
  0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
  0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
  0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
  0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
  0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
  0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
  0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
  0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
  0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
  0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
  0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
  0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
  0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
  0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
  0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
  0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/* Round constants for key expansion */
static const uint8_t aes_rcon[10] = {
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* Galois Field GF(2^8) multiplication by 2 */
static inline uint8_t xtime(uint8_t x)
{
  return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

/* Expands a 32-byte (256-bit) key into 240 bytes of round keys */
static void aes256_set_key(aes256_ctx_t *ctx, const uint8_t key[32])
{
  uint8_t temp[4];
  int i, rcon_idx = 0;

  memcpy(ctx->round_keys, key, 32);

  for (i = 32; i < AES256_EXP_KEY_SIZE; i += 4) {
    temp[0] = ctx->round_keys[i - 4];
    temp[1] = ctx->round_keys[i - 3];
    temp[2] = ctx->round_keys[i - 2];
    temp[3] = ctx->round_keys[i - 1];

    if (i % 32 == 0) {
      /* RotWord + SubWord + Rcon */
      uint8_t t = temp[0];
      temp[0] = aes_sbox[temp[1]] ^ aes_rcon[rcon_idx++];
      temp[1] = aes_sbox[temp[2]];
      temp[2] = aes_sbox[temp[3]];
      temp[3] = aes_sbox[t];
    } else if (i % 32 == 16) {
      /* SubWord only */
      temp[0] = aes_sbox[temp[0]];
      temp[1] = aes_sbox[temp[1]];
      temp[2] = aes_sbox[temp[2]];
      temp[3] = aes_sbox[temp[3]];
    }

    ctx->round_keys[i + 0] = ctx->round_keys[i - 32] ^ temp[0];
    ctx->round_keys[i + 1] = ctx->round_keys[i - 31] ^ temp[1];
    ctx->round_keys[i + 2] = ctx->round_keys[i - 30] ^ temp[2];
    ctx->round_keys[i + 3] = ctx->round_keys[i - 29] ^ temp[3];
  }
}

/* Encrypts one 16-byte block from `in` to `out` */
static void aes256_encrypt_block(const aes256_ctx_t *ctx,
                                 const uint8_t in[16], uint8_t out[16])
{
  uint8_t state[16];
  const uint8_t *rk = ctx->round_keys;
  int round, i;

  /* AddRoundKey (Round 0) */
  for (i = 0; i < 16; i++) {
      state[i] = in[i] ^ rk[i];
  }
  rk += 16;

  /* Rounds 1 to 13 */
  for (round = 1; round < AES256_ROUNDS; round++) {
    uint8_t t0, t1, t2, t3;

    /* SubBytes + ShiftRows */
    t0 = aes_sbox[state[0]];  t1 = aes_sbox[state[5]];
    t2 = aes_sbox[state[10]]; t3 = aes_sbox[state[15]];

    state[4] = aes_sbox[state[4]];   state[5] = aes_sbox[state[9]];
    state[6] = aes_sbox[state[14]];  state[7] = aes_sbox[state[3]];

    state[8] = aes_sbox[state[8]];   state[9] = aes_sbox[state[13]];
    state[10] = aes_sbox[state[2]];  state[11] = aes_sbox[state[7]];

    state[12] = aes_sbox[state[12]]; state[13] = aes_sbox[state[1]];
    state[14] = aes_sbox[state[6]];  state[15] = aes_sbox[state[11]];

    state[0] = t0; state[1] = t1; state[2] = t2; state[3] = t3;

    /* MixColumns + AddRoundKey */
    for (i = 0; i < 16; i += 4) {
      uint8_t a0 = state[i], a1 = state[i + 1], a2 = state[i + 2], a3 = state[i + 3];
      uint8_t h0 = xtime(a0 ^ a1);
      uint8_t h1 = xtime(a1 ^ a2);
      uint8_t h2 = xtime(a2 ^ a3);
      uint8_t h3 = xtime(a3 ^ a0);
      uint8_t all = a0 ^ a1 ^ a2 ^ a3;

      state[i + 0] = a0 ^ all ^ h0 ^ rk[i + 0];
      state[i + 1] = a1 ^ all ^ h1 ^ rk[i + 1];
      state[i + 2] = a2 ^ all ^ h2 ^ rk[i + 2];
      state[i + 3] = a3 ^ all ^ h3 ^ rk[i + 3];
    }
    rk += 16;
  }

  /* Final Round (Round 14): SubBytes + ShiftRows + AddRoundKey (No MixColumns) */
  out[0]  = aes_sbox[state[0]]  ^ rk[0];
  out[1]  = aes_sbox[state[5]]  ^ rk[1];
  out[2]  = aes_sbox[state[10]] ^ rk[2];
  out[3]  = aes_sbox[state[15]] ^ rk[3];

  out[4]  = aes_sbox[state[4]]  ^ rk[4];
  out[5]  = aes_sbox[state[9]]  ^ rk[5];
  out[6]  = aes_sbox[state[14]] ^ rk[6];
  out[7]  = aes_sbox[state[3]]  ^ rk[7];

  out[8]  = aes_sbox[state[8]]  ^ rk[8];
  out[9]  = aes_sbox[state[13]] ^ rk[9];
  out[10] = aes_sbox[state[2]]  ^ rk[10];
  out[11] = aes_sbox[state[7]]  ^ rk[11];

  out[12] = aes_sbox[state[12]] ^ rk[12];
  out[13] = aes_sbox[state[1]]  ^ rk[13];
  out[14] = aes_sbox[state[6]]  ^ rk[14];
  out[15] = aes_sbox[state[11]] ^ rk[15];
}

static void aes_ctr_process_frame(uint8_t *data, size_t len,
                                  na4_ctr_state_t *ctr,
                                  const aes256_ctx_t *aes_ctx)
{
  uint8_t keystream[16];
  size_t i, chunk;

  while (len > 0) {
    /* Generate 16 bytes of keystream by encrypting the current counter blk */
    aes256_encrypt_block(aes_ctx, ctr->block, keystream);

    /* XOR input buffer in-place */
    chunk = (len < 16) ? len : 16;
    for (i = 0; i < chunk; i++) {
        data[i] ^= keystream[i];
    }

    /* Advance the 32-bit counter for the next 16-byte block */
    ctr_increment(ctr);

    data += chunk;
    len -= chunk;
  }
}

int main(int argc, char **argv)
{
  struct na4_context na4_ctx = {};
  struct na4_context *ctx = &na4_ctx;
  int decode_enabled = 0;
  int secret_bytes = -1;
  int key_bytes;
  int i = 1;

  while(1) {
    if (argc >= (i + 1)) {
      if (strcmp(argv[i], "--help") == 0) {
        fprintf(stdout, "%s: a simple Base77 encoder/decoder\n", argv[0]);
        return 0;
      } else if (strcmp(argv[i], "-s") == 0) {
        if ((argc >= (i + 2))) {
          if (sscanf(argv[i + 1], "%u", &secret_bytes) == 1) {
            ctx->sha256_enabled = 1;
            i += 2;
          }
        }
        if (secret_bytes == -1) {
          fprintf(stderr, "%s: unable to parse -s argument\n", argv[0]);
          return 1;
        }
        continue;
      } else if (strcmp(argv[i], "-d") == 0) {
        decode_enabled = 1;
        i++;
        continue;
      } else if (strcmp(argv[i], "-e") == 0) {
        ctx->aes256_enabled = 1;
        i++;
        continue;
      }
    }
    break;
  }

  if (secret_bytes >= 0) {
    if (secret_bytes == 0) {
      fprintf(stderr, "warning: using potentially unsafe 0-byte secret\n");
    }

    if (ctx->aes256_enabled) {
      if (decode_enabled) {
        key_bytes = stdin_fread_secret(&na4_ctx, init_secret,
                                       secret_bytes, process_secret,
                                       finish_secret_decrypt);
      } else {
        key_bytes = stdin_fread_secret(&na4_ctx, init_secret,
                                       secret_bytes, process_secret,
                                       finish_secret_encrypt);
      }
    } else {
      key_bytes = stdin_fread_secret(&na4_ctx, init_secret,
                                     secret_bytes, process_secret,
                                     finish_secret_plaintext);
    }

    if (key_bytes != secret_bytes) {
      fprintf(stderr, "error: unable to read secret (%d, %d)\n",
              key_bytes, secret_bytes);
      return 1;
    }
  }

  if (ctx->aes256_enabled && !ctx->sha256_enabled) {
    fprintf(stderr,
            "error: unable to use crypto without a secret (-s / -e)\n");
    return 1;
  }

  if (decode_enabled) {
    return stdin_fread(&na4_ctx, NULL,
                       OUTPUT_BUFSIZE, decode_frame,
                       compare_signature) < 0;
  } else {
    return stdin_fread(&na4_ctx, NULL,
                       INPUT_BUFSIZE, encode_frame,
                       store_signature) < 0;
  }
}
