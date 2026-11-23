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

static void print_bytes(const uint8_t *data, int length,
			char *space, char *newline)
{
  int m;

  for (m = 0; m < length; m++) {
    printf("%s%02x%s", m % 16 ? space : "",
           data[m],
           (m % 16) == 15 ? newline : "");
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
  // AES S-Box Constants
  // FIPS 197 (November 26, 2001)
  // file nist.fips.197.pdf (52 pages)
  // AES Algorithm Specification Cipher on page 16 (PDF 21)
  {
    printf("\n\nFIPS 197\n");
    printf("5. Algorithm Specification\n");
    printf("5.1 Cipher\n");
    printf("5.1.1 SubBytes() Transformation\n\n");
    print_bytes(&aes_sbox[0], ARRAY_SIZE(aes_sbox), " ", "\n");
    printf("\nFigure 7. S-box: substitution values");
    printf(" for the byte xy (in hexadecimal format).\n");
    printf("\n\n");
  }

  // AES Round constants
  // FIPS 197 (May 9, 2023)
  // file NIST.FIPS.197-upd1.pdff (46 pages)
  // AES Algorithm Specification Cipher on page 17 (PDF 25)
  {
    printf("\n\nFIPS 197\n");
    printf("5. Algorithm Specification\n");
    printf("5.2 KEYEXPANSION()\n\n");
    printf("Table 5. Round constants\n");
    print_bytes(&aes_rcon[0], ARRAY_SIZE(aes_rcon), " ", "");
    printf("\n\n");
  }

  // AES-256 Key Expansion
  // FIPS 197 (November 26, 2001)
  // file nist.fips.197.pdf (52 pages)
  // AES-256 Key Expansion Examples on page 30-32 (PDF 35-37)
  {
    AES_CTX aes_ctx = {};
    uint8_t key[32] = { 0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
                        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
                        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
                        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4 };

    printf("\n\nFIPS 197\n");
    printf("Appendix A - Key Expansion Examples\n");
    printf("A.3 Expansion of a 256-bit Cipher Key\n");
    printf("\nCipher Key =\n");
    print_bytes(&key[0], ARRAY_SIZE(key), " ", "\n");
    printf("\n");
    aes256_set_key(&aes_ctx, key);
    printf("#i (dec) w[i] = temp XOR w[i - Nk]\n");
    print_i_words(8, &aes_ctx.round_keys[0], ARRAY_SIZE(aes_ctx.round_keys));
    printf("\n");
  }

  // AES-256 Example Vectors
  // FIPS 197 (November 26, 2001)
  // file nist.fips.197.pdf (52 pages)
  // AES-256 Example Vectors on Page 42-43 (PDF 47-48)
  {
    AES_CTX aes_ctx = {};
    uint8_t key[32] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
    uint8_t data_in[16] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                            0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
    uint8_t data_out[16] = {};

    printf("\n\nFIPS 197\n");
    printf("Appendix C - Example Vectors\n");
    printf("C.3 AES-256 (Nk=8,Nr=14)\n\n");
    printf("PLAINTEXT: ");
    print_bytes(&data_in[0], ARRAY_SIZE(data_in), "", "");
    printf("\n");
    printf("KEY:       ");
    print_bytes(&key[0], ARRAY_SIZE(key), "", "");
    printf("\n\n");
    aes256_set_key(&aes_ctx, key);
    printf("CIPHER (ENCRYPT):\n");
    printf("round[ 0].input  ");
    print_bytes(&data_in[0], ARRAY_SIZE(data_in), "", "");
    aes256_encrypt_block( &aes_ctx, data_in, &data_out[0]);
    printf("\nround[14].output ");
    print_bytes(&data_out[0], ARRAY_SIZE(data_out), "", "");
    printf("\n\n");
  }
  
  return 0;
}
