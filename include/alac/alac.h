/*
 * ALAC (Apple Lossless Audio Codec) decoder
 * Copyright (c) 2005 David Hammerton
 * All rights reserved.
 *
 * http://crazney.net/programs/itunes/alac.html
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __ALAC__DECOMP_H
#define __ALAC__DECOMP_H

#include <stdint.h>

typedef struct alac_file alac_file;

alac_file *alac_create(int samplesize, int numchannels);
void alac_decode_frame(alac_file *alac, unsigned char *inbuffer, void *outbuffer, int *outputsize);
void alac_set_info(alac_file *alac, char *inputbuffer);
void alac_allocate_buffers(alac_file *alac);
void alac_free(alac_file *alac);

struct alac_file {
  unsigned char *input_buffer;
  int input_buffer_bitaccumulator; /* used so we can do arbitrary
                                      bit reads */

  int samplesize;
  int numchannels;
  int bytespersample;

  /* buffers */
  int32_t *predicterror_buffer_a;
  int32_t *predicterror_buffer_b;

  int32_t *outputsamples_buffer_a;
  int32_t *outputsamples_buffer_b;

  int32_t *uncompressed_bytes_buffer_a;
  int32_t *uncompressed_bytes_buffer_b;

  /* stuff from setinfo */
  uint32_t setinfo_max_samples_per_frame; /* 0x1000 = 4096 */ /* max samples per frame? */
  uint8_t setinfo_7a;                                         /* 0x00 */
  uint8_t setinfo_sample_size;                                /* 0x10 */
  uint8_t setinfo_rice_historymult;                           /* 0x28 */
  uint8_t setinfo_rice_initialhistory;                        /* 0x0a */
  uint8_t setinfo_rice_kmodifier;                             /* 0x0e */
  uint8_t setinfo_7f;                                         /* 0x02 */
  uint16_t setinfo_80;                                        /* 0x00ff */
  uint32_t setinfo_82; /* 0x000020e7 */                       /* max sample size?? */
  uint32_t setinfo_86; /* 0x00069fe4 */                       /* bit rate (avarge)?? */
  uint32_t setinfo_8a_rate;                                   /* 0x0000ac44 */
  /* end setinfo stuff */
};

#endif /* __ALAC__DECOMP_H */
