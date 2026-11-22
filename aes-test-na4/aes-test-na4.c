/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* aes-test-na4.c - a simple test bench for the AES code in na4              */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* Makes use of the following software components [known working revision]:  */
/* - na4 (https://github.com/papadamm/random/) [commit b4a7b45]              */
/*                                                                           */
/* To test, compile and run this program and compare output with FIPS 197    */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(n) sizeof(n) / sizeof(n[0])

#define __STANDALONE__
#include "../na4/na4.c"

/* yes, the following functions are unused, silence the compiler */
void *unused_functions[] = {
  store_signature, compare_signature,
  encode_frame, decode_frame, read_data,
  read_secret, init_secret, process_secret,
  finish_secret_plaintext, finish_secret_encrypt, finish_secret_decrypt };

static void print_bytes(const uint8_t *data, int length)
{
  int m;

  for (m = 0; m < length; m++) {
    printf("%s%02x%s", m % 16 ? " " : "", data[m], (m % 16) == 15 ? "\n" : "");
  }
}

static void print_i_words(int i, const uint8_t *data, int length)
{
  int m;

  for (m = i * 4; m < length; m += 4) {
    printf("%d %02x%02x%02x%02x\n", m / 4,
	   data[m + 0], data[m + 1], data[m + 2], data[m + 3]);
  }
}

int main(void)
{
  AES_CTX aes_ctx = {};

  // Constants
  printf("aes_rcon[]:\n");
  print_bytes(&aes_rcon[0], ARRAY_SIZE(aes_rcon));
  printf("\n\n");

  printf("aes_sbox[]:\n");
  print_bytes(&aes_sbox[0], ARRAY_SIZE(aes_sbox));
  printf("\n");

  // FIPS 197 Appendix A.3 Expansion of a 256-bit Key
  {
    uint8_t key[32] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
                        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
                        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };

    printf("Key =\n");
    print_bytes(&key[0], ARRAY_SIZE(key));
    printf("\n");
    aes256_set_key(&aes_ctx, key);
    printf("#i (dec) w[i] = temp XOR w[i - Nk]\n");
    print_i_words(8, &aes_ctx.round_keys[0], ARRAY_SIZE(aes_ctx.round_keys));
    printf("\n");
  }
  
  return 0;
}
