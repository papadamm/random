/* SPDX-License-Identifier: MIT */
/*                                                                           */
/* 1654.c - a simple tool to encode/decode base16 and base54 data            */
/*                                                                           */
/* Copyright (C) 2026 Magnus Damm                                            */
/*                                                                           */
/* base54 allows using 5.6875 bits per character in two different modes      */
/*                                                                           */
/* single mode: encode one byte of data into two characters ASCII with CRC   */
/* dual mode: encode two bytes of data into three characters ASCII           */
/*                                                                           */
/* single mode is equally efficient as base16 but adds CRC for robustness    */
/* dual mode is less efficient than base64 but allows for base16 mixed use   */
/*                                                                           */
/* single mode also supports an out-of-band bitstream to optionally embed an */
/* user specific data stream. this stream may for instance be handy to send  */
/* some low bandwidth data like LCD bitmap data, song lyrics or perhaps even */
/* as a control interface when streaming audio over the main base54 link     */
/*                                                                           */
/* since the regular hexadecimal character set is omitted from base54 this   */
/* reserves space for yet another side channel opportunity                   */
/*                                                                           */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

char hex[] = "0123456789ABCDEF0123456789abcdef";
char fivefour[] = "GHIJKLMNOPQRSTUVWXYZghijklmnopqrstuvwxyz(),-.<>@[]^_{}";

static char encode_lower_hex_char(uint8_t value)
{
  return hex[value + 16]; /* hex[16] = '0', hex[31] = 'f' */
}

static char encode_upper_hex_char(uint8_t value)
{
  return hex[value]; /* hex[0] = '0', hex[15] = 'F' */
}

/* encode binary input stream to hexadecimal ASCII character output */
static int encode_hex(char (*encode)(uint8_t))
{
  uint8_t buf[1];

  while (fread(&buf[0], sizeof(buf[0]), 1, stdin)) {
    printf("%c", encode(buf[0] >> 4));
    printf("%c", encode(buf[0] & 0x0f));
  }

  return 0;
}

static int decode_hex_char(int ch)
{
  const char *str = &hex[0];
  char *found;

  found = strchr(str, ch);
  if (found) {
    return (found - str) & 0x0f; /* both upper and lower characters */
  }

  return -1;
}

/* decode hex character stream, both lower and upper case ASCII supported */
static int decode_hex(void)
{
  uint8_t in_buf[1];
  uint8_t out_buf[1];
  int high_nibble = 1;
  int prev_data = 0;
  int this_data;

  while (fread(&in_buf[0], sizeof(in_buf[0]), 1, stdin)) {
    if (high_nibble) {
      prev_data = decode_hex_char(in_buf[0]);
    } else {
      if (prev_data >= 0) {
	this_data = decode_hex_char(in_buf[0]);
	if (this_data >= 0) {
	  out_buf[0] = (prev_data << 4) | this_data;
	  fwrite(&out_buf[0], sizeof(out_buf[0]), 1, stdout);
	} else {
	  fprintf(stderr, "unable to decode the lower nibble hex character\n");
	  return 1;
	}
      } else {
	fprintf(stderr, "unable to decode the upper nibble hex character\n");
	return 1;
      }
    }
    high_nibble ^= 0x01;
  }

  if (high_nibble != 1) {
    fprintf(stderr, "uneven amount of hex character\n");
    return 1;
  }
  
  return 0;
}

/* generate an ASCII character using the case sensitive 54 character set */
static char encode_54_char(uint8_t value)
{
  return fivefour[value];
}

/* the first encoded 54 character has three embedded flags in single mode */
static uint8_t encode_54s_v1(uint8_t msv, int oob, int crc, int mode)
{
  return (msv << 3) | (oob << 2) | (crc << 1) | (mode << 0);
}

/* the first encoded 54 character has one embedded flag in dual mode */
static uint8_t encode_54d_v1(uint8_t msv, int oob, int crc, int mode)
{
  return (msv << 1) | (mode << 0);
}

static int calculate_54s_crc(uint8_t v1, uint8_t v2)
{
  uint16_t data_in = (v2 << 8) | (v1 << 0);
  uint8_t cnt = 0;

  /* highly unoptimized hamming weight (replace with one processor opcode) */
  while (data_in) {
    if (data_in & 0x01) {
      cnt++;
    }
    data_in >>= 1;
  }

  return cnt;
}

static void encode_54s_byte(uint8_t byte, int oob)
{
  uint8_t msv, v1, crc, v2;
    
  v2 = byte % 54;
  msv = byte / 54;
    
  v1 = encode_54s_v1(msv, oob, 0, 0); /* mode = 0, two char single mode */
  crc = calculate_54s_crc(v1, v2) & 0x01;
  v1 = encode_54s_v1(msv, oob, crc, 0);
    
  printf("%c", encode_54_char(v1));
  printf("%c", encode_54_char(v2));
}

static void encode_54s_byte_no_oob(uint8_t byte)
{
  encode_54s_byte(byte, 0);
}

/* encode binary input stream to single mode 54 ASCII character output */
static int encode_54s(int (*get_oob)(void *oob_data), void *oob_data)
{
  uint8_t buf[1];
  int oob = 0;

  while (fread(&buf[0], sizeof(buf[0]), 1, stdin)) {
    if (get_oob) {
      oob = get_oob(oob_data);
    }

    encode_54s_byte(buf[0], oob);
  }

  return 0;
}

/* encode binary input stream to dual mode 54 ASCII character output */
static int encode_54d(void (*encode_single)(uint8_t byte))
{
  uint8_t buf[1];
  int char_nr = 0;
  int prev_data = 0;
  uint16_t tmp16, tmp;
  uint8_t msv, v1, v2, v3;

  while (fread(&buf[0], sizeof(buf[0]), 1, stdin)) {
    if (char_nr == 0) {
      prev_data = buf[0];
    } else {

      tmp16 = (prev_data << 8) | buf[0];

      v3 = tmp16 % 54;
      tmp = tmp16 / 54;
      v2 = tmp % 54;
      msv = tmp / 54;
      
      v1 = encode_54d_v1(msv, 0, 0, 1); /* mode = 1, three char dual mode */
    
      printf("%c", encode_54_char(v1));
      printf("%c", encode_54_char(v2));
      printf("%c", encode_54_char(v3));
    }

    char_nr ^= 0x01;
  }

  if (encode_single) {
    encode_single(prev_data);
  } else {
    if (char_nr != 0) {
      fprintf(stderr, "uneven amount of input characters to encode as 54\n");
      return 1;
    }
  }
  
  return 0;
}

static int decode_54_char(int ch)
{
  const char *str = &fivefour[0];
  char *found;

  found = strchr(str, ch);
  if (found) {
    return found - str;
  }

  return -1;
}

/* decode single 54 and/or dual 54 depending on flags */
static int decode_54(int allow_single, int allow_dual,
		     void (*put_oob)(void *oob_data, int v), void *oob_data)
{
  uint8_t in_buf[1];
  uint8_t out_buf[1];
  int char_nr = 0;
  int v1 = 0;
  int v2 = 0;
  int v3;
  uint16_t tmp16;
  uint8_t crc;

  while (fread(&in_buf[0], sizeof(in_buf[0]), 1, stdin)) {
    if (char_nr == 0) {
      v1 = decode_54_char(in_buf[0]);
      char_nr++;
    } else if (char_nr == 1) {
      v2 = decode_54_char(in_buf[0]);

      if ((v1 < 0) || (v2 < 0)) {
	fprintf(stderr, "unknown first or second 54 characters\n");
	return 1;
      }

      if ((v1 & (1 << 0)) == 0) { /* mode == 0 (single mode) */
	if (allow_single) {
	  crc = calculate_54s_crc(v1 & 0xfd, v2); /* omit CRC bit */
	  if (((v1 & (1 << 1)) >> 1) != (crc & 0x01)) {
	    fprintf(stderr, "crc mismatch\n");
	    return 1;
	  }

	  if (put_oob) {
	    put_oob(oob_data, (v1 * (1 << 2) >> 2));
	  }

	  /* most significant value is stored in the first character */
	  out_buf[0] = ((v1 >> 3) * 54) + v2;
	  fwrite(&out_buf[0], sizeof(out_buf[0]), 1, stdout);
	} else {
	  fprintf(stderr, "unable to decode second 54 character\n");
	  return 1;
	}
	
	char_nr = 0;
      } else {
	char_nr++;
      }
    } else if (char_nr == 2) {

      if (allow_dual) {
	v3 = decode_54_char(in_buf[0]);

	if (v3 < 0) {
	  fprintf(stderr, "unknown third 54 character\n");
	  return 1;
	}
      
	tmp16 = ((v1 >> 1) * 54 * 54) + (v2 * 54) + v3;
	out_buf[0] = tmp16 >> 8;
	fwrite(&out_buf[0], sizeof(out_buf[0]), 1, stdout);
	out_buf[0] = tmp16 & 0xff;
	fwrite(&out_buf[0], sizeof(out_buf[0]), 1, stdout);
	char_nr = 0;
      } else {
	fprintf(stderr, "unable to decode third 54 character\n");
	return 1;
      }
    }
  }

  if (char_nr != 0) {
    fprintf(stderr, "uneven amount of 54 characters to decode\n");
    return 1;
  }
  
  return 0;
}

int main(int argc, char **argv)
{
  FILE *fp_usage = stdout;

  if (argc == 2) {
    if (strcmp(argv[1], "encode-x16") == 0) {
      return encode_hex(encode_lower_hex_char);
    }
    if (strcmp(argv[1], "encode-X16") == 0) {
      return encode_hex(encode_upper_hex_char);
    }
    if (strcmp(argv[1], "decode-xX16") == 0) {
      return decode_hex();
    }
    if (strcmp(argv[1], "encode-54s") == 0) {
      return encode_54s(NULL, NULL);
    }
    if (strcmp(argv[1], "encode-54d") == 0) {
      return encode_54d(NULL);
    }
    if (strcmp(argv[1], "encode-54ds") == 0) {
      return encode_54d(encode_54s_byte_no_oob);
    }
    if (strcmp(argv[1], "decode-54s") == 0) {
      return decode_54(1, 0, NULL, NULL);
    }
    if (strcmp(argv[1], "decode-54d") == 0) {
      return decode_54(0, 1, NULL, NULL);
    }
    if (strcmp(argv[1], "decode-54ds") == 0) {
      return decode_54(1, 1, NULL, NULL);
    }

    fprintf(stderr, "%s: missing command\n\n", argv[0]);
    fp_usage = stderr;
  }

  fprintf(fp_usage, "%s: a simple 1654 encoder/decoder tool using stdio\n",
	  argv[0]);

  fprintf(fp_usage, "usage: %s command\n", argv[0]);
  fprintf(fp_usage, "supported commands:\n");
  fprintf(fp_usage, "  encode-x16 - encode lower case hex\n");
  fprintf(fp_usage, "  encode-X16 - encode upper case hex\n");
  fprintf(fp_usage, "  decode-xX16 - decode case insensitive hex\n");
  fprintf(fp_usage, "  encode-54s - encode 54 single mode\n");
  fprintf(fp_usage, "  encode-54d - encode 54 dual mode\n");
  fprintf(fp_usage, "  encode-54ds - encode 54 dual and 54 single mode\n");
  fprintf(fp_usage, "  decode-54s - decode 54 single mode\n");
  fprintf(fp_usage, "  decode-54d - decode 54 dual mode\n");
  fprintf(fp_usage, "  decode-54ds - decode 54 dual and 54 single mode\n");
}
