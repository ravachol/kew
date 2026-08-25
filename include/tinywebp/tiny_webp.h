/*

  # tiny_webp.h

  Reasonably tiny, single-header WebP library in C99.

  ## Examples

  The API is explained in more detail below, but here are some quick examples:

  Load RGB/RGBA:

    // If you already have the file in memory, use twp_read_from_memory() instead.
    int width, height;
    unsigned char *data = twp_read("path/to/file.webp",
                                   &width, &height,
                                   twp_FORMAT_RGBA, // or twp_FORMAT_RGB
                                   0); // no flags
    if (data) { } // Do something with the data
    free(data);

  Get information about a .webp file:

    // If you already have the file in memory, use twp_get_info_from_memory() instead.
    int width, height, lossless, alpha;
    int ok = twp_get_info("path/to/file.webp", &width, &height, &lossless, &alpha);
    if (ok) { } // Do something with the info

  Load YUV/YUVA:

    // This will return NULL if the image is lossless, use twp_get_info() to check first.
    // Note that WebP is 4:2:0, so width and height will be half for chroma.
    int width, height;
    unsigned char *data = twp_read("path/to/file.webp",
                                   &width, &height,
                                   twp_FORMAT_YUV, // or twp_FORMAT_YUVA
                                   0); // no flags
    if (data) {
        unsigned char *y, *u, *v;
        int luma_stride, chroma_stride;
        twp_unpack_yuv(data, width, height,
                       &y, &u, &v, NULL, // alpha plane is NULL since we want YUV
                       &luma_stride, &chroma_stride, NULL); // alpha stride is NULL
        // Do something with the YUV data
        free(data); // y, u and v are now invalid. Do not free() them!
    }

  ## Compilation

  Copy tiny_webp.h into your project, then #define twp_IMPLEMENTATION in exactly one C/C++ file
  that #includes tiny_webp.h. For example:

    #include <stdio.h>
    #include <stdlib.h>
    #define twp_IMPLEMENTATION
    #include "tiny_webp.h"

  You can also do the #define twp_IMPLEMENTATION part at the end of a file. That way you don't see
  all the private symbols in your autocomplete:

    #include "tiny_webp.h"

    int main()
    {
        ...
    }

    #define twp_IMPLEMENTATION
    #include "tiny_webp.h"

  Alternatively, you could also just create a .c file that is empty except for the #define and the
  #include.

  ## API

    unsigned char *twp_read(const char *file_path, int *width, int *height,
                            twp_format format, twp_flags flags)

  Loads a .webp image from a file. You must free the result.

  Returns NULL on error.

  Formats:

  * twp_FORMAT_RGBA
  * twp_FORMAT_RGB
  * twp_FORMAT_YUV
  * twp_FORMAT_YUVA

  YUV and YUVA will return NULL for lossless images. Use twp_get_info() to first check if you have
  a lossless or a lossy image.

  For YUV and YUVA, you must call twp_unpack_yuv() on the result.

  Flags:

  * twp_FLAG_SKIP_LOOP_FILTER: Skips loop filtering, which saves some decoding time, but slightly
  lowers quality.

  --------

    unsigned char *twp_read_from_memory(void *data, int data_len, int *width, int *height,
                                        twp_format format, twp_flags flags)

  The same as twp_read(), except it reads from memory instead.

  --------

    void twp_unpack_yuv(unsigned char *ptr, int width, int height,
                        unsigned char **y, unsigned char **u, unsigned char **v, unsigned char **a,
                        int *luma_stride, int *chroma_stride, int *alpha_stride)

  If you loaded an image and requested either YUV or YUVA as the format, you must call this
  function to unpack the returned pointer into the individual planes and get the strides.

  Do not free the returned Y, U, V, A pointers. Just free the original pointer returned from
  twp_read(). Y, U, V and A will be valid for as long as the original pointer is valid.

  You can pass NULL if you don't care about a certain plane. For example, if you requested
  format YUV, there is no alpha, so pass NULL for the a and alpha_stride parameters.

  --------

    int twp_get_info(const char *file_path, int *width, int *height, int *lossless, int *alpha)

  Get information about a .webp file.

  You can pass NULL if you don't care about something.

  Returns 0 on error and 1 on success.

  --------

    int twp_get_info_from_memory(void *data, int data_len,
                                 int *width, int *height, int *lossless, int *alpha)

  The same as twp_get_info(), except it reads from memory instead.

  ## Compile Time Options

  Simply #define any of these. twp_STATIC needs to be seen by the header and the implementation.
  For all the others, only the implementation needs to see the #define. I recommend just putting
  them at the top of tiny_webp.h.

  * twp_STATIC: Define extern function to be static instead.
  * twp_NO_SIMD: Disable all SIMD optimizations.
  * twp_FORCE_SSE2: If for some reason SSE2 support wasn't correctly detected, you can force-
  enable it.
  * twp_SIGNED_RIGHT_SHIFT_FIX: This library heavily relies on right-shifting negative integers to
  compile to arithmetic shifts. Enable this option if your compiler does not guarantee this (which
  is unlikely, all major compilers do).

  ## Current Limitations

  * Probably not as fast as libwebp
  * No encoding
  * No animations
  * SIMD optimizations are SSE2 only
  * License is not 0BSD, which would be my preference (but that's not my fault, see the next
  section)

  ## License

  As far as I understand, it's actually impossible to release an implementation of the WebP spec
  under anything more permissive than BSD3. The reason for this is that the spec uses C code
  everywhere, meaning if you read the code and then write an implementation, you have created a
  derivative work. If the spec was written purely in English, this would not be a problem, because
  implementing something described in English does not count as a derivative work of that
  description. The spec further says that "the bitstream is defined by the reference source code
  and not this narrative." So, really, the source code is the spec, and therefore any
  implementation of the spec is a derivative work.

  It seems to me, then, that a bunch of WebP/WebM/VP8 implementations floating around the web are
  quite openly violating the license.

  If Google could re-license the spec to 0BSD or something, that would be great.


  Copyright (c) 2010, 2011, Google Inc. All rights reserved.
  Copyright (c) 2025, justus2510
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
  
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
  
    * Neither the name of Google nor the names of its contributors may
      be used to endorse or promote products derived from this software
      without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  
*/


#ifndef twp__HEADER_GUARD
#define twp__HEADER_GUARD

#ifdef twp_STATIC
    #define twp__STORAGE static
#else
    #ifdef __cplusplus
        #define twp__STORAGE extern "C"
    #else
        #define twp__STORAGE extern
    #endif
#endif

typedef int twp_flags;
#define twp_FLAG_SKIP_LOOP_FILTER (1 << 0)

typedef enum {
    twp_FORMAT_RGBA,
    twp_FORMAT_RGB,
    twp_FORMAT_YUV,
    twp_FORMAT_YUVA
} twp_format;

twp__STORAGE unsigned char *twp_read(const char *file_path, int *width, int *height,
                                     twp_format format, twp_flags flags);

twp__STORAGE unsigned char *twp_read_from_memory(void *data, int data_len, int *width,
                                    int *height, twp_format format, twp_flags flags);

twp__STORAGE void twp_unpack_yuv(unsigned char *ptr, int width, int height,
                    unsigned char **y, unsigned char **u, unsigned char **v, unsigned char **a,
                    int *luma_stride, int *chroma_stride, int *alpha_stride);

twp__STORAGE int twp_get_info(const char *file_path, int *width, int *height,
                              int *lossless, int *alpha);

twp__STORAGE int twp_get_info_from_memory(void *data, int data_len, int *width,
                                          int *height, int *lossless, int *alpha);

#endif




#if defined(twp_IMPLEMENTATION) && !defined(twp__IMPL_GUARD)
#define twp__IMPL_GUARD

#if defined(__GNUC__) || defined(__clang__)
    #define twp__INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
    #define twp__INLINE __forceinline
#else
    #define twp__INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define twp__trap() __builtin_trap()
#elif defined(_MSC_VER)
    #define twp__trap() __debugbreak()
#else
    #define twp__trap() do { *(int *)0 = 0; } while (0)
#endif

#ifdef twp__ENABLE_ASSERTS
#define twp__assert(x) do { if (!(x)) {fprintf(stdout, "Assert failed (line %i): %s\n", __LINE__, #x); twp__trap();} } while (0)
#else
#define twp__assert(x) do { } while (0)
#endif

#ifndef twp_NO_SIMD
    #if defined(twp_FORCE_SSE2) || defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86) && defined(_M_IX86_FP) && (_M_IX86_FP >= 2))
        #define twp__SSE2
    #endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef twp__SSE2
#include <emmintrin.h>
#endif

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFF
#define twp__64BIT
#elif UINTPTR_MAX == 0xFFFFFFFF
#define twp__32BIT
#else
#define twp__32BIT // idk, just use 32-bit i guess?
#endif

#define twp__arrlen(x) ((int)(sizeof(x) / sizeof((x)[0])))

#define twp__MIN_FILE_SIZE 24

#define twp__MAX_VP8L_HUFFMAN_SYMBOL (256 + 24 + (1 << 11))
#define twp__MAX_VP8L_HUFFMAN_CODE_LENGTH 15

// from some rough testing, 8 seems good
// must be lower than 15 because 0xF encodes a special value in the huffman entries (see below)
#define twp__HUFFMAN_SPLIT 8
#define twp__HUFFMAN_SPLIT_MASK ((1 << twp__HUFFMAN_SPLIT) - 1)

#define twp__INVALID_HUFFMAN_SYMBOL_MASK 0xFFF0

typedef struct {
    void *data;
    int size;
} twp__chunk;

typedef struct {
    twp__chunk VP8X;
    twp__chunk VP8L;
    twp__chunk VP8;
    twp__chunk ALPH;
} twp__chunk_table;

typedef struct {
    unsigned char *data;
    int num_bytes;
    int at_byte;
#ifdef twp__64BIT
    uint64_t bits;
#else
    uint32_t bits;
#endif
    int num_bits;
} twp__bit_reader;

typedef struct twp__huffman_table {
    //
    // least significant 4 bits: the code length. since the maximum is 15 (most likely it's lower because of splitting), this fits.
    // most significant 12 bits: the actual symbol. the maximum symbol value requires 12 bits, so this also fits.
    //  _ _ _ _ _ _ _ _ _ _ _ _|_ _ _ _
    // msb                     |     lsb
    //    symbol (12 bits)       code length (4 bits)
    //
    // if the code length is 0xF, then symbol is instead an index into subtables
    //
    // if symbol == 0xFFF, then this entry is unoccupied and accessing it is an error
    // 0xFFF is way above twp__MAX_VP8L_HUFFMAN_SYMBOL so this is fine
    //
    int max_code_length;
    uint16_t *entries;
    struct twp__huffman_table *subtables;
    int num_subtables;
    int max_subtables;
} twp__huffman_table;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} twp__rgba8;

enum {
    twp__TRANSFORM_PREDICTOR = 0,
    twp__TRANSFORM_COLOR = 1,
    twp__TRANSFORM_SUBTRACT_GREEN = 2,
    twp__TRANSFORM_COLOR_INDEXING = 3,
    twp__TRANSFORM_COUNT = 4
};

typedef struct {
    int type;

    struct {
        int pow2;
        twp__rgba8 *img;
        int width;
        int height;
    } pred_col; // both the predictor and color transform use this

    struct {
        twp__rgba8 *color_table;
        int color_table_size;
        int orig_width;
        int width_divider;
        int divided_width;
    } color_idxing;
} twp__transform;

typedef struct {
    int exists;
    int pow2;
    int size;
    twp__rgba8 *cache;
} twp__color_cache;

typedef struct {
    int exists;
    int pow2;
    int width;
    int height;
    twp__rgba8 *img;
} twp__meta_prefix_img;

typedef struct {
    twp__huffman_table arr[5];
} twp__prefix_code_group;

static const int twp__dist_mapping[120][2] = {
    {0, 1},  {1, 0},  {1, 1},  {-1, 1}, {0, 2},  {2, 0},  {1, 2},
    {-1, 2}, {2, 1},  {-2, 1}, {2, 2},  {-2, 2}, {0, 3},  {3, 0},
    {1, 3},  {-1, 3}, {3, 1},  {-3, 1}, {2, 3},  {-2, 3}, {3, 2},
    {-3, 2}, {0, 4},  {4, 0},  {1, 4},  {-1, 4}, {4, 1},  {-4, 1},
    {3, 3},  {-3, 3}, {2, 4},  {-2, 4}, {4, 2},  {-4, 2}, {0, 5},
    {3, 4},  {-3, 4}, {4, 3},  {-4, 3}, {5, 0},  {1, 5},  {-1, 5},
    {5, 1},  {-5, 1}, {2, 5},  {-2, 5}, {5, 2},  {-5, 2}, {4, 4},
    {-4, 4}, {3, 5},  {-3, 5}, {5, 3},  {-5, 3}, {0, 6},  {6, 0},
    {1, 6},  {-1, 6}, {6, 1},  {-6, 1}, {2, 6},  {-2, 6}, {6, 2},
    {-6, 2}, {4, 5},  {-4, 5}, {5, 4},  {-5, 4}, {3, 6},  {-3, 6},
    {6, 3},  {-6, 3}, {0, 7},  {7, 0},  {1, 7},  {-1, 7}, {5, 5},
    {-5, 5}, {7, 1},  {-7, 1}, {4, 6},  {-4, 6}, {6, 4},  {-6, 4},
    {2, 7},  {-2, 7}, {7, 2},  {-7, 2}, {3, 7},  {-3, 7}, {7, 3},
    {-7, 3}, {5, 6},  {-5, 6}, {6, 5},  {-6, 5}, {8, 0},  {4, 7},
    {-4, 7}, {7, 4},  {-7, 4}, {8, 1},  {8, 2},  {6, 6},  {-6, 6},
    {8, 3},  {5, 7},  {-5, 7}, {7, 5},  {-7, 5}, {8, 4},  {6, 7},
    {-6, 7}, {7, 6},  {-7, 6}, {8, 5},  {7, 7},  {-7, 7}, {8, 6},
    {8, 7},
};

twp__INLINE static int twp__sra(int x, int n)
{
#ifdef twp_SIGNED_RIGHT_SHIFT_FIX
    if (x < 0)
        return ~(~x >> n);
    else
        return x >> n;
#else
    return x >> n;
#endif
}

static int twp__check_fourcc(unsigned char *fourcc, const char *want)
{
    for (int i = 0; i < 4; ++i) {
        if (fourcc[i] != want[i])
            return 0;
    }
    return 1;
}

twp__INLINE static int twp__div_round_up(int num, int den)
{
    return (num + den - 1) / den;
}

twp__INLINE static int twp__abs(int n)
{
    return (n < 0) ? -n : n;
}

twp__INLINE static int twp__clamp(int n, int low, int high)
{
    if (n < low)
        return low;
    else if (n > high)
        return high;
    else
        return n;
}

twp__INLINE static twp__rgba8 twp__make_rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    twp__rgba8 result;
    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;
    return result;
}

twp__INLINE static void twp__refill_bits(twp__bit_reader *reader)
{
#ifdef twp__64BIT
    while (reader->num_bits <= 56 && reader->at_byte < reader->num_bytes) {
        uint64_t byte = reader->data[reader->at_byte++];
        reader->bits |= byte << reader->num_bits;
        reader->num_bits += 8;
    }
#else
    while (reader->num_bits <= 24 && reader->at_byte < reader->num_bytes) {
        uint32_t byte = reader->data[reader->at_byte++];
        reader->bits |= byte << reader->num_bits;
        reader->num_bits += 8;
    }
#endif
}

twp__INLINE static int twp__peek_bits_unsafe(twp__bit_reader *reader, int n)
{
    int result = (int)(reader->bits & ((1 << n) - 1));
    return result;
}

twp__INLINE static void twp__consume_bits(twp__bit_reader *reader, int n)
{
    reader->bits >>= n;
    reader->num_bits -= n;
}

twp__INLINE static int twp__read_bits(twp__bit_reader *reader, int n)
{
    twp__assert(n >= 1 && n <= 31);

    if (reader->num_bits < n) {
        twp__refill_bits(reader);
        if (reader->num_bits < n)
            return -1;
    }
    int result = twp__peek_bits_unsafe(reader, n);
    twp__consume_bits(reader, n);

    return result;
}

static uint16_t *twp__allocate_huffman_entries(int count)
{
    uint16_t *entries = (uint16_t *)malloc(count * sizeof(*entries));
    for (int i = 0; i < count; ++i)
        entries[i] = twp__INVALID_HUFFMAN_SYMBOL_MASK;
    return entries;
}

static void twp__construct_simple_huffman_table(int a, int b, twp__huffman_table *table)
{
    // this is needed for simple huffman codes because in that case the symbols not implicit
    // in the order of the code length array, as is assumed in construct_huffman_table().
    // however, this is also used in the special case when construct_huffman_table() is
    // called with only one entry. not technically needed, but easier

    table->subtables = NULL;
    table->max_subtables = 0;
    table->num_subtables = 0;

    twp__assert(a >= 0);
    if (b != -1 && a > b) {
        int tmp = a;
        a = b;
        b = tmp;
    }

    if (b == -1) {
        table->max_code_length = 0;
        table->entries = twp__allocate_huffman_entries(1);
        table->entries[0] = (uint16_t)(a << 4);
    } else {
        table->max_code_length = 1;
        table->entries = twp__allocate_huffman_entries(2);
        table->entries[0] = (uint16_t)(1 | (a << 4));
        table->entries[1] = (uint16_t)(1 | (b << 4));
    }
}

static void twp__free_huffman_table(twp__huffman_table *table)
{
    for (int i = 0; i < table->num_subtables; ++i)
        twp__free_huffman_table(table->subtables + i);
    free(table->subtables);
    free(table->entries);
    memset(table, 0, sizeof(*table));
}

static int twp__insert_huffman_table_entry(twp__huffman_table *table, int code, int code_length,
                                           int symbol, int max_subtable_code_length)
{
    if (code_length <= table->max_code_length) {
        int num_trash_bits = table->max_code_length - code_length;
        int num_bits_to_fill = 1 << num_trash_bits;
        for (int j = 0; j < num_bits_to_fill; ++j) {
            int idx = code | (j << code_length);
            if (idx >= (1 << table->max_code_length)) return 0;
            if (table->entries[idx] != twp__INVALID_HUFFMAN_SYMBOL_MASK) return 0;

            table->entries[idx] = (uint16_t)code_length | ((uint16_t)symbol << 4);
        }

        return 1;
    } else {
        int code_first_part = code & twp__HUFFMAN_SPLIT_MASK;
        if (code_first_part >= (1 << table->max_code_length))
            return 0;

        twp__huffman_table *sub;
        if (((table->entries[code_first_part] & 0xF)) == 0xF) { // subtable already exists
            uint16_t entry = table->entries[code_first_part];
            uint16_t idx = entry >> 4;
            twp__assert(idx < (1 << twp__HUFFMAN_SPLIT));
            twp__assert(idx < table->num_subtables);
            sub = table->subtables + idx;
        } else { // no subtable yet, insert one
            if (table->entries[code_first_part] != twp__INVALID_HUFFMAN_SYMBOL_MASK)
                return 0;

            twp__assert(table->num_subtables < (1 << twp__HUFFMAN_SPLIT));
            if (table->num_subtables >= table->max_subtables) {
                table->max_subtables = table->max_subtables ? table->max_subtables*2 : 4;
                table->subtables = (twp__huffman_table *)realloc(table->subtables, table->max_subtables * sizeof(twp__huffman_table));
            }

            int subtable_idx = table->num_subtables++;
            sub = table->subtables + subtable_idx;
            memset(sub, 0, sizeof(*sub));
            sub->max_code_length = max_subtable_code_length;
            sub->entries = twp__allocate_huffman_entries(1 << max_subtable_code_length);
            table->entries[code_first_part] = (uint16_t)((subtable_idx << 4) | 0xF);
        }

        return twp__insert_huffman_table_entry(sub, code >> twp__HUFFMAN_SPLIT, code_length - twp__HUFFMAN_SPLIT, symbol, 0);
    }
}

static int twp__construct_huffman_table(int *code_lengths, int num_code_lengths, twp__huffman_table *table)
{
    if (num_code_lengths <= 0)
        return 0;

    int max_code_length = 0;
    int nz_sym = -1;
    int code_length_histogram[twp__MAX_VP8L_HUFFMAN_CODE_LENGTH + 1] = {0};
    for (int i = 0; i < num_code_lengths; ++i) {
        if (!code_lengths[i])
            continue;
        nz_sym = i;
        ++code_length_histogram[code_lengths[i]];
        if (code_lengths[i] > max_code_length)
            max_code_length = code_lengths[i];
    }

    if ((max_code_length == 1) && (code_length_histogram[1] == 1)) {
        // special case when we have only one entry in the huffman table
        twp__assert(nz_sym != -1);
        twp__construct_simple_huffman_table(nz_sym, -1, table);
        return 1;
    }

    if (max_code_length > twp__MAX_VP8L_HUFFMAN_CODE_LENGTH)
        return 0; // should not be possible anyway

    int max_main_table_code_length;
    int max_subtable_code_length;
    if (max_code_length > twp__HUFFMAN_SPLIT) {
        max_main_table_code_length = twp__HUFFMAN_SPLIT;
        max_subtable_code_length = max_code_length - twp__HUFFMAN_SPLIT;
    } else {
        max_main_table_code_length = max_code_length;
        max_subtable_code_length = 0;
    }

    table->max_code_length = max_main_table_code_length;
    table->entries = twp__allocate_huffman_entries(1 << max_main_table_code_length);
    table->num_subtables = 0;
    table->max_subtables = 0;
    table->subtables = NULL;

    int next_code[twp__MAX_VP8L_HUFFMAN_CODE_LENGTH + 1] = {0};
    int current_code = 0;
    for (int code_length = 1; code_length <= twp__MAX_VP8L_HUFFMAN_CODE_LENGTH; ++code_length) {
        current_code = (current_code + code_length_histogram[code_length - 1]) << 1;
        next_code[code_length] = current_code;
    }

    for (int symbol = 0; symbol < num_code_lengths; ++symbol) {
        int code_length = code_lengths[symbol];
        if (code_length == 0) continue;
        twp__assert(code_length <= twp__MAX_VP8L_HUFFMAN_CODE_LENGTH);
        twp__assert(symbol <= twp__MAX_VP8L_HUFFMAN_SYMBOL); // 12 bits

        int code = next_code[code_length];
        int reversed_code = 0;
        for (int j = 0; j < code_length; ++j) {
            int bit = (code >> j) & 1;
            reversed_code |= bit << (code_length - j - 1);
        }

        if (!twp__insert_huffman_table_entry(table, reversed_code, code_length, symbol, max_subtable_code_length)) {
            twp__free_huffman_table(table);
            return 0;
        }

        ++next_code[code_length];
    }

    // we don't actually know what the max code length for a subtable is until we built it
    for (int i = 0; i < table->num_subtables; ++i) {
        twp__huffman_table *sub = table->subtables + i;
        int new_max_code_length = 0;
        for (int j = 0; j < 1 << sub->max_code_length; ++j) {
            if ((sub->entries[j] & 0xF) > new_max_code_length)
                new_max_code_length = sub->entries[j] & 0xF;
        }
        sub->max_code_length = new_max_code_length;
    }

    return 1;
}

twp__INLINE static int twp__huffman_read(twp__bit_reader *reader, twp__huffman_table *ht)
{
    if (ht->max_code_length == 0) return ht->entries[0] >> 4;

    // refill
    if (reader->num_bits < twp__MAX_VP8L_HUFFMAN_CODE_LENGTH) twp__refill_bits(reader);

    // step 1
    int bits = twp__peek_bits_unsafe(reader, ht->max_code_length);
    uint16_t entry = ht->entries[bits];

    // step 2, if needed
    if ((entry & 0xF) == 0xF) {
        twp__consume_bits(reader, twp__HUFFMAN_SPLIT);
        ht = &ht->subtables[entry >> 4];
        bits = twp__peek_bits_unsafe(reader, ht->max_code_length);
        entry = ht->entries[bits];
    }

    if ((entry & twp__INVALID_HUFFMAN_SYMBOL_MASK) == twp__INVALID_HUFFMAN_SYMBOL_MASK) return -1;
    twp__consume_bits(reader, entry & 0xF);
    if (reader->num_bits < 0) return -1;
    return entry >> 4;
}

twp__INLINE static int twp__get_meta_prefix_code(twp__rgba8 pix)
{
    return ((int)pix.r << 8) | (int)pix.g;
}

twp__INLINE static int twp__hash_pixel(twp__rgba8 pix, int color_cache_pow2)
{
    uint32_t color32 = ((uint32_t)pix.a << 24) | ((uint32_t)pix.r << 16) | ((uint32_t)pix.g << 8) | ((uint32_t)pix.b << 0);
    uint32_t hash = (0x1e35a7bd * color32) >> (32 - color_cache_pow2);
    return (int)hash;
}

twp__INLINE static int twp__decode_lz77(twp__bit_reader *reader, int code)
{
    if (code < 0) {
        return -1;
    } else if (code < 4) {
        return code + 1;
    } else {
        int num_extra_bits = (code - 2) >> 1;
        int extra_bits = twp__read_bits(reader, num_extra_bits);
        if (extra_bits == -1) return -1;

        int offset = (2 + (code & 1)) << num_extra_bits;
        return offset + extra_bits + 1;
    }
}

static int twp__read_prefix_code(twp__bit_reader *reader, int color_cache_size, int group_idx, twp__huffman_table *result)
{
    int is_simple = twp__read_bits(reader, 1);
    if (is_simple == -1) return 0;

    if (is_simple) {
        int num_symbols = twp__read_bits(reader, 1) + 1;
        if (num_symbols == 0) return 0;

        int first_is_8bits = twp__read_bits(reader, 1);
        if (first_is_8bits == -1) return 0;

        int sym0 = twp__read_bits(reader, first_is_8bits ? 8 : 1);
        if (sym0 == -1) return 0;

        int sym1 = -1;
        if (num_symbols == 2) {
            sym1 = twp__read_bits(reader, 8);
            if (sym1 == -1) return 0;
        }

        twp__construct_simple_huffman_table(sym0, sym1, result);
        return 1;
    } else {
        // the huffman code lengths themselves are huffman coded

        int num_stored_code_length_code_lengths = twp__read_bits(reader, 4) + 4;
        if (num_stored_code_length_code_lengths == 3) return 0;

        // i think the reason this uses a weird ordering like that is because that way
        // it's more likely you can just skip the last ones?
        enum { NUM_CODE_LENGTH_CODE_LENGTHS = 19 };
        static const int code_length_code_order[NUM_CODE_LENGTH_CODE_LENGTHS] = {
            17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        };

        int code_length_code_lengths[NUM_CODE_LENGTH_CODE_LENGTHS] = {0};
        for (int j = 0; j < num_stored_code_length_code_lengths; ++j) {
            int bits = twp__read_bits(reader, 3);
            if (bits == -1) return 0;

            code_length_code_lengths[code_length_code_order[j]] = bits;
        }

        // if the "mode" bit is 1, that means we should NOT decode until we have the entire
        // alphabet (the alphabet size is specified by the current index of the group, see the
        // switch statement below).
        // instead, we decode exactly n times, and the rest of the code lengths will be 0.

        int num_code_length_symbols; // this is the alphabet size
        switch (group_idx) {
            case 0: {
                num_code_length_symbols = 256 + 24 + color_cache_size;
            } break;

            case 1:
            case 2:
            case 3: {
                num_code_length_symbols = 256;
            } break;

            case 4: {
                num_code_length_symbols = 40;
            } break;

            default: {
                twp__assert(0);
                return 0;
            } break;
        }

        int mode = twp__read_bits(reader, 1);
        if (mode == -1) return 0;

        int num_code_length_symbols_to_decode;
        if (mode == 0) {
            num_code_length_symbols_to_decode = -1;
        } else {
            int bits_to_read = twp__read_bits(reader, 3);
            if (bits_to_read == -1) return 0;
            bits_to_read = 2 + 2*bits_to_read;

            num_code_length_symbols_to_decode = twp__read_bits(reader, bits_to_read) + 2;
            if (num_code_length_symbols_to_decode == 1) return 0;
        }

        twp__huffman_table code_lengths_ht;
        if (!twp__construct_huffman_table(code_length_code_lengths, NUM_CODE_LENGTH_CODE_LENGTHS, &code_lengths_ht))
            return 0;

        int ok = 0;
        int *code_lengths = (int *)calloc(num_code_length_symbols, sizeof(*code_lengths));
        int count = 0;
        for (;;) {
            if (count >= num_code_length_symbols) break;
            if (num_code_length_symbols_to_decode == 0) break;

            int bits = twp__huffman_read(reader, &code_lengths_ht);
            if (bits == -1) goto end;
            if (num_code_length_symbols_to_decode != -1) --num_code_length_symbols_to_decode;

            if (bits <= 15) {
                code_lengths[count++] = bits;
            } else if (bits == 16) {
                int prev_non_zero = 8;
                for (int i = count - 1; i >= 0; --i) {
                    if (code_lengths[i] != 0) {
                        prev_non_zero = code_lengths[i];
                        break;
                    }
                }

                int repeat_count = twp__read_bits(reader, 2);
                if (repeat_count == -1) goto end;
                repeat_count += 3;
                if (count + repeat_count > num_code_length_symbols) goto end;

                for (int i = 0; i < repeat_count; ++i)
                    code_lengths[count++] = prev_non_zero;
            } else if (bits == 17 || bits == 18) {
                int repeat_count = twp__read_bits(reader, (bits == 17) ? 3 : 7);
                if (repeat_count == -1) goto end;
                repeat_count += (bits == 17) ? 3 : 11;
                if (count + repeat_count > num_code_length_symbols) goto end;

                for (int i = 0; i < repeat_count; ++i)
                    code_lengths[count++] = 0;
            } else {
                twp__assert(0);
                goto end;
            }
        }

        twp__assert(count <= num_code_length_symbols);
        if (num_code_length_symbols_to_decode != 0 && num_code_length_symbols_to_decode != -1)
            goto end;

        if (!twp__construct_huffman_table(code_lengths, num_code_length_symbols, result))
            goto end;

        ok = 1;

end:
        free(code_lengths);
        twp__free_huffman_table(&code_lengths_ht);
        return ok;
    }
}

static int twp__read_prefix_code_group(twp__bit_reader *reader, int color_cache_size, twp__prefix_code_group *result)
{
    for (int i = 0; i < 5; ++i) {
        if (!twp__read_prefix_code(reader, color_cache_size, i, result->arr + i)) {
            for (int j = 0; j < i; ++j)
                twp__free_huffman_table(result->arr + j);
            return 0;
        }
    }
    return 1;
}

static twp__rgba8 *twp__read_coded_image(twp__bit_reader *reader, int width, int height, int main)
{
    twp__rgba8 *result = NULL;
    twp__color_cache color_cache;
    twp__meta_prefix_img meta_prefix;
    twp__prefix_code_group *prefix_code_groups = NULL;
    int num_prefix_code_groups = 0;
    int num_pix = width * height;

    memset(&color_cache, 0, sizeof(color_cache));
    memset(&meta_prefix, 0, sizeof(meta_prefix));

    color_cache.exists = twp__read_bits(reader, 1);
    if (color_cache.exists == -1) goto err;

    if (color_cache.exists) {
        color_cache.pow2 = twp__read_bits(reader, 4);
        if (color_cache.pow2 < 1 || color_cache.pow2 > 11) goto err;

        color_cache.size = 1 << color_cache.pow2;
        color_cache.cache = (twp__rgba8 *)calloc(color_cache.size, sizeof(twp__rgba8));
    }

    if (main) { // only the main image has meta prefix codes, sub images do not
        meta_prefix.exists = twp__read_bits(reader, 1);
        if (meta_prefix.exists == -1) goto err;

        if (meta_prefix.exists) {
            meta_prefix.pow2 = twp__read_bits(reader, 3) + 2;
            if (meta_prefix.pow2 == 1) goto err;

            meta_prefix.width = twp__div_round_up(width, 1 << meta_prefix.pow2);
            meta_prefix.height = twp__div_round_up(height, 1 << meta_prefix.pow2);
            meta_prefix.img = twp__read_coded_image(reader, meta_prefix.width, meta_prefix.height, 0);
            if (!meta_prefix.img) goto err;
        }
    }

    if (meta_prefix.exists) {
        for (int i = 0; i < meta_prefix.width * meta_prefix.height; ++i) {
            twp__rgba8 pix = meta_prefix.img[i];
            int mpc = twp__get_meta_prefix_code(pix);
            if (mpc > num_prefix_code_groups)
                num_prefix_code_groups = mpc;
        }
    }
    ++num_prefix_code_groups;

    prefix_code_groups = (twp__prefix_code_group *)malloc(num_prefix_code_groups * sizeof(twp__prefix_code_group));
    for (int i = 0; i < num_prefix_code_groups; ++i) {
        if (!twp__read_prefix_code_group(reader, color_cache.size, prefix_code_groups + i))
            goto err;
    }

    result = (twp__rgba8 *)malloc(num_pix * 4);
    for (int i = 0; i < num_pix;) {
        int x = i % width;
        int y = i / width;

        twp__prefix_code_group *prefix_code_group;
        if (meta_prefix.exists) {
            int position = (y >> meta_prefix.pow2) * meta_prefix.width + (x >> meta_prefix.pow2);
            if (position >= meta_prefix.width * meta_prefix.height) goto err;

            int code = twp__get_meta_prefix_code(meta_prefix.img[position]);
            if (code >= num_prefix_code_groups) goto err;

            prefix_code_group = prefix_code_groups + code;
        } else {
            prefix_code_group = prefix_code_groups;
        }

        int S = twp__huffman_read(reader, &prefix_code_group->arr[0]);
        if (S < 0) goto err;
        if (S > 256 + 24 + color_cache.size - 1) goto err;

        if (S < 256) {
            int r = twp__huffman_read(reader, &prefix_code_group->arr[1]);
            int g = S;
            int b = twp__huffman_read(reader, &prefix_code_group->arr[2]);
            int a = twp__huffman_read(reader, &prefix_code_group->arr[3]);
            if (r == -1 || b == -1 || a == -1) goto err;

            twp__rgba8 pix = twp__make_rgba8((uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a);

            if (color_cache.exists) {
                int hash = twp__hash_pixel(pix, color_cache.pow2);
                if (hash < 0 || hash >= color_cache.size) goto err;
                color_cache.cache[hash] = pix;
            }

            result[i++] = pix;
        } else if (S < 256 + 24) {
            int length_code = S - 256;
            int length = twp__decode_lz77(reader, length_code);
            if (length == -1) goto err;

            int dist_code = twp__huffman_read(reader, &prefix_code_group->arr[4]);
            int dist = twp__decode_lz77(reader, dist_code);
            if (dist == -1) goto err;

            if (dist > 120) {
                dist -= 120;
            } else {
                // this is safe to do because decode_lz77() always returns at least 1 (except when erroring, which is checked above)
                int dx = twp__dist_mapping[dist - 1][0];
                int dy = twp__dist_mapping[dist - 1][1];
                dist = dy*width + dx;
                if (dist < 1) dist = 1;
            }

            if (dist > i) goto err;
            if (i + length > num_pix) goto err;

            for (int j = 0; j < length; ++j)
                result[i + j] = result[i - dist + j];

            if (color_cache.exists) {
                for (int j = 0; j < length; ++j) {
                    twp__rgba8 pix = result[i + j];
                    int hash = twp__hash_pixel(pix, color_cache.pow2);
                    if (hash < 0 || hash >= color_cache.size) goto err;
                    color_cache.cache[hash] = pix;
                }
            }

            i += length;
        } else {
            if (!color_cache.exists) goto err;

            int color_cache_idx = S - (256 + 24);
            if (color_cache_idx < 0 || color_cache_idx >= color_cache.size) goto err;

            result[i++] = color_cache.cache[color_cache_idx];
        }
    }

    goto end;

err:
    free(result);
    result = NULL;

end:
    free(color_cache.cache);
    free(meta_prefix.img);
    for (int i = 0; i < num_prefix_code_groups; ++i) {
        for (int j = 0; j < 5; ++j) {
            twp__free_huffman_table(&prefix_code_groups[i].arr[j]);
        }
    }
    free(prefix_code_groups);
    return result;
}

static void twp__free_transforms(twp__transform *tfs, int count)
{
    for (int i = 0; i < count; ++i) {
        twp__transform *tf = tfs + i;

        switch (tf->type) {
            case twp__TRANSFORM_PREDICTOR:
            case twp__TRANSFORM_COLOR: {
                free(tf->pred_col.img);
            } break;

            case twp__TRANSFORM_SUBTRACT_GREEN: {
                // nothing to free
            } break;

            case twp__TRANSFORM_COLOR_INDEXING: {
                free(tf->color_idxing.color_table);
            } break;

            default: {
                twp__assert(0);
            } break;
        }

        memset(tf, 0, sizeof(*tf));
    }
}

static int twp__read_transform(twp__bit_reader *reader, twp__transform *tf, int *img_width,
                               int img_height, int *already_seen, int *finished)
{
    *finished = 0;

    int have_transform = twp__read_bits(reader, 1);
    if (have_transform == -1) return 0;
    if (have_transform == 0) {
        *finished = 1;
        return 1;
    }

    int type = twp__read_bits(reader, 2);
    if (type == -1) return 0;
    if (already_seen[type]) return 0;
    already_seen[type] = 1;
    tf->type = type;

    switch (type) {
        case twp__TRANSFORM_PREDICTOR:
        case twp__TRANSFORM_COLOR: {
            tf->pred_col.pow2 = twp__read_bits(reader, 3) + 2;
            if (tf->pred_col.pow2 == 1) return 0;
            tf->pred_col.width = twp__div_round_up(*img_width, 1 << tf->pred_col.pow2);
            tf->pred_col.height = twp__div_round_up(img_height, 1 << tf->pred_col.pow2);
            tf->pred_col.img = twp__read_coded_image(reader, tf->pred_col.width, tf->pred_col.height, 0);
            if (!tf->pred_col.img) return 0;
        } break;

        case twp__TRANSFORM_SUBTRACT_GREEN: {
            // no data
        } break;

        case twp__TRANSFORM_COLOR_INDEXING: {
            tf->color_idxing.orig_width = *img_width;
            tf->color_idxing.color_table_size = twp__read_bits(reader, 8) + 1;

            if (tf->color_idxing.color_table_size == 0) return 0;
            else if (tf->color_idxing.color_table_size <= 2) tf->color_idxing.width_divider = 1 << 3;
            else if (tf->color_idxing.color_table_size <= 4) tf->color_idxing.width_divider = 1 << 2;
            else if (tf->color_idxing.color_table_size <= 16) tf->color_idxing.width_divider = 1 << 1;
            else tf->color_idxing.width_divider = 1;

            tf->color_idxing.color_table = twp__read_coded_image(reader, tf->color_idxing.color_table_size, 1, 0);
            if (!tf->color_idxing.color_table) return 0;

            // the color table is delta coded
            for (int i = 1; i < tf->color_idxing.color_table_size; ++i) {
                tf->color_idxing.color_table[i].r += tf->color_idxing.color_table[i-1].r;
                tf->color_idxing.color_table[i].g += tf->color_idxing.color_table[i-1].g;
                tf->color_idxing.color_table[i].b += tf->color_idxing.color_table[i-1].b;
                tf->color_idxing.color_table[i].a += tf->color_idxing.color_table[i-1].a;
            }

            tf->color_idxing.divided_width = twp__div_round_up(*img_width, tf->color_idxing.width_divider);
            *img_width = tf->color_idxing.divided_width;
        } break;

        default: {
            twp__assert(0);
            return 0;
        } break;
    }

    return 1;
}

static int twp__read_transforms(twp__bit_reader *reader, twp__transform *tfs, int img_width, int img_height)
{
    int count = 0;
    int already_seen[twp__TRANSFORM_COUNT] = {0};
    int finished = 0;
    while (!finished) {
        if (count >= 4 || !twp__read_transform(reader, tfs + count, &img_width, img_height, already_seen, &finished)) {
            twp__free_transforms(tfs, count);
            return -1;
        }
        if (!finished)
            ++count;
    }
    return count;
}

twp__INLINE static twp__rgba8 twp__avg2(twp__rgba8 a, twp__rgba8 b)
{
    twp__rgba8 res;
    res.r = (a.r + b.r) / 2;
    res.g = (a.g + b.g) / 2;
    res.b = (a.b + b.b) / 2;
    res.a = (a.a + b.a) / 2;
    return res;
}

twp__INLINE static int twp__color_transform_delta(int t, int c)
{
    t = (int8_t)t;
    c = (int8_t)c;
    return (int8_t)((t * c) >> 5);
}

static twp__rgba8 *twp__reverse_transform(twp__rgba8 *img, int width, int height, twp__transform *tf)
{
    switch (tf->type) {
        case twp__TRANSFORM_PREDICTOR: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int i = y*width + x;

                    twp__rgba8 pred;
                    if (i == 0) {
                        pred = twp__make_rgba8(0, 0, 0, 255);
                    } else if (y == 0) {
                        pred = img[i - 1];
                    } else if (x == 0) {
                        pred = img[i - width];
                    } else {
                        twp__rgba8 left = img[i - 1];
                        twp__rgba8 top = img[i - width];
                        twp__rgba8 top_left = img[i - width - 1];
                        twp__rgba8 top_right = img[i - width + 1];

                        int subimg_i = (y >> tf->pred_col.pow2)*tf->pred_col.width + (x >> tf->pred_col.pow2);
                        int pred_mode = tf->pred_col.img[subimg_i].g;
                        if (pred_mode >= 14) {
                            free(img);
                            return NULL;
                        }

                        switch (pred_mode) {
                            case 0: {
                                pred.r = 0;
                                pred.g = 0;
                                pred.b = 0;
                                pred.a = 255;
                            } break;

                            case 1: pred = left; break;
                            case 2: pred = top; break;
                            case 3: pred = top_right; break;
                            case 4: pred = top_left; break;
                            case 5: pred = twp__avg2(twp__avg2(left, top_right), top); break;
                            case 6: pred = twp__avg2(left, top_left); break;
                            case 7: pred = twp__avg2(left, top); break;
                            case 8: pred = twp__avg2(top_left, top); break;
                            case 9: pred = twp__avg2(top, top_right); break;
                            case 10: pred = twp__avg2(twp__avg2(left, top_left), twp__avg2(top, top_right)); break;

                            case 11: {
                                int pr = left.r + top.r - top_left.r;
                                int pg = left.g + top.g - top_left.g;
                                int pb = left.b + top.b - top_left.b;
                                int pa = left.a + top.a - top_left.a;

                                // manhattan distances
                                int dist_left = twp__abs(pr - left.r) + twp__abs(pg - left.g) + twp__abs(pb - left.b) + twp__abs(pa - left.a);
                                int dist_top = twp__abs(pr - top.r) + twp__abs(pg - top.g) + twp__abs(pb - top.b) + twp__abs(pa - top.a);

                                if (dist_left < dist_top)
                                    pred = left;
                                else
                                    pred = top;
                            } break;

                            case 12: {
                                pred.r = (uint8_t)twp__clamp(left.r + top.r - top_left.r, 0, 255);
                                pred.g = (uint8_t)twp__clamp(left.g + top.g - top_left.g, 0, 255);
                                pred.b = (uint8_t)twp__clamp(left.b + top.b - top_left.b, 0, 255);
                                pred.a = (uint8_t)twp__clamp(left.a + top.a - top_left.a, 0, 255);
                            } break;

                            case 13: {
                                twp__rgba8 a = twp__avg2(left, top);
                                twp__rgba8 b = top_left;

                                pred.r = (uint8_t)twp__clamp(a.r + ((a.r - b.r) / 2), 0, 255);
                                pred.g = (uint8_t)twp__clamp(a.g + ((a.g - b.g) / 2), 0, 255);
                                pred.b = (uint8_t)twp__clamp(a.b + ((a.b - b.b) / 2), 0, 255);
                                pred.a = (uint8_t)twp__clamp(a.a + ((a.a - b.a) / 2), 0, 255);
                            } break;

                            default: {
                                twp__assert(0);
                                memset(&pred, 0, sizeof(pred));
                            } break;
                        }
                    }

                    img[i].r += pred.r;
                    img[i].g += pred.g;
                    img[i].b += pred.b;
                    img[i].a += pred.a;
                }
            }

            return img;
        } break;

        case twp__TRANSFORM_COLOR: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    int i = y*width + x;
                    int subimg_i = (y >> tf->pred_col.pow2)*tf->pred_col.width + (x >> tf->pred_col.pow2);

                    twp__rgba8 pix = img[i];
                    twp__rgba8 sub_pix = tf->pred_col.img[subimg_i];

                    int red_to_blue = sub_pix.r;
                    int green_to_blue = sub_pix.g;
                    int green_to_red = sub_pix.b;

                    int tmp_red = pix.r;
                    int tmp_blue = pix.b;

                    tmp_red += twp__color_transform_delta(green_to_red, pix.g);
                    tmp_blue += twp__color_transform_delta(green_to_blue, pix.g);
                    tmp_blue += twp__color_transform_delta(red_to_blue, tmp_red);

                    img[i].r = (uint8_t)tmp_red;
                    img[i].b = (uint8_t)tmp_blue;
                }
            }

            return img;
        } break;

        case twp__TRANSFORM_SUBTRACT_GREEN: {
            for (int i = 0; i < width*height; ++i) {
                img[i].r += img[i].g;
                img[i].b += img[i].g;
            }

            return img;
        } break;

        case twp__TRANSFORM_COLOR_INDEXING: {
            if (tf->color_idxing.width_divider == 1) {
                // we can skip a bunch of work if there isn't any bit-packing
                for (int i = 0; i < width*height; ++i) {
                    int color_table_idx = img[i].g;
                    if (color_table_idx >= tf->color_idxing.color_table_size)
                        img[i] = twp__make_rgba8(0, 0, 0, 0);
                    else
                        img[i] = tf->color_idxing.color_table[color_table_idx];
                }

                return img;
            } else {
                twp__assert(tf->color_idxing.width_divider == 2 || tf->color_idxing.width_divider == 4 || tf->color_idxing.width_divider == 8);
                twp__assert(width == tf->color_idxing.divided_width);

                // note that at the end of each row there can be unused bits,
                // this happens when orig_width % width_divider != 0

                int bits_per_idx = 8 / tf->color_idxing.width_divider;
                int idx_bit_mask = (1 << bits_per_idx) - 1;
                twp__rgba8 *new_img = (twp__rgba8 *)malloc(tf->color_idxing.orig_width * height * 4);

                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < tf->color_idxing.orig_width; ++x) {
                        int packed = img[y*tf->color_idxing.divided_width + x/tf->color_idxing.width_divider].g;
                        int color_table_idx = (packed >> ((x % tf->color_idxing.width_divider) * bits_per_idx)) & idx_bit_mask;
                        if (color_table_idx >= tf->color_idxing.color_table_size)
                            new_img[y*tf->color_idxing.orig_width + x] = twp__make_rgba8(0, 0, 0, 0);
                        else
                            new_img[y*tf->color_idxing.orig_width + x] = tf->color_idxing.color_table[color_table_idx];
                    }
                }

                free(img);
                return new_img;
            }
        } break;

        default: {
            twp__assert(0);
            return img;
        } break;
    }
}

static int twp__read_vp8l_header(twp__bit_reader *reader, int *width, int *height)
{
    int magic = twp__read_bits(reader, 8);
    if (magic != 0x2f) return 0;

    *width = twp__read_bits(reader, 14) + 1;
    *height = twp__read_bits(reader, 14) + 1;
    if (*width <= 0 || *height <= 0) return 0;

    int alpha_is_used = twp__read_bits(reader, 1); // doesn't actually do anything
    if (alpha_is_used == -1) return 0;

    int version = twp__read_bits(reader, 3);
    if (version != 0) return 0;

    return 1;
}

static unsigned char *twp__read_vp8l(void *data, int data_len, int *width, int *height, twp_format format, int is_vp8_alpha)
{
    if (format == twp_FORMAT_YUV || format == twp_FORMAT_YUVA)
        return NULL;

    twp__bit_reader reader;
    memset(&reader, 0, sizeof(reader));
    reader.data = (unsigned char *)data;
    reader.num_bytes = data_len;

    if (!is_vp8_alpha && !twp__read_vp8l_header(&reader, width, height))
        return NULL;

    twp__transform transforms[twp__TRANSFORM_COUNT];
    int num_transforms = twp__read_transforms(&reader, transforms, *width, *height);
    if (num_transforms == -1) return NULL;

    // check if we have a color indexing transform; is so, we need to use its subsampled width
    // for all image data that is processed *before* this transform is reversed
    int divided_width = *width;
    for (int i = 0; i < num_transforms; ++i) {
        twp__transform *tf = transforms + i;
        if (tf->type == twp__TRANSFORM_COLOR_INDEXING) {
            divided_width = tf->color_idxing.divided_width;
            break;
        }
    }

    twp__rgba8 *img = twp__read_coded_image(&reader, divided_width, *height, 1);

    int got_citf = 0;
    for (int i = num_transforms - 1; img && i >= 0; --i) { // this gets skipped if twp__read_coded_image returned NULL
        // we need to check if we already reversed a color indexing transform; if so, we need
        // to stop using the subsampled width and start using the original width
        twp__transform *tf = transforms + i;

        int width_to_use = got_citf ? *width : divided_width;
        img = twp__reverse_transform(img, width_to_use, *height, tf);
        if (!img) break;

        if (tf->type == twp__TRANSFORM_COLOR_INDEXING) {
            twp__assert(!got_citf);
            got_citf = 1;
        }
    }
    twp__free_transforms(transforms, num_transforms);

    if (img && format == twp_FORMAT_RGB) {
        unsigned char *rgb = (unsigned char *)malloc(*width * *height * 3);
        unsigned char *ptr = rgb;
        for (int i = 0; i < *width * *height; ++i) {
            *ptr++ = img[i].r;
            *ptr++ = img[i].g;
            *ptr++ = img[i].b;
        }
        free(img);
        return rgb;
    } else {
        return (unsigned char *)img;
    }
}

//
// vp8 decoding
//

// msvc does not optimize this endian-load pattern: https://developercommunity.visualstudio.com/t/Missed-optimization:-loadstore-coalesci/987039
// gcc and clang compile this to a mov+bswap, which is what we want.
// since msvc sucks, we have to resort to a hacky solution to get it to generate the correct code.

#ifdef twp__64BIT
    typedef uint64_t twp__arith_dec_type;
    #define twp__ARITH_DEC_SHIFT 56
    #define twp__ARITH_DEC_READ_HELPER(buf) ((uint64_t)(buf)[7]<<0) | ((uint64_t)(buf)[6]<<8) | ((uint64_t)(buf)[5]<<16) | ((uint64_t)(buf)[4]<<24) | ((uint64_t)(buf)[3]<<32) | ((uint64_t)(buf)[2]<<40) | ((uint64_t)(buf)[1]<<48) | ((uint64_t)(buf)[0]<<56);
#else
    typedef uint32_t twp__arith_dec_type;
    #define twp__ARITH_DEC_SHIFT 24
    #define twp__ARITH_DEC_READ_HELPER(buf) ((uint32_t)(buf)[3]<<0) | ((uint32_t)(buf)[2]<<8) | ((uint32_t)(buf)[1]<<16) | ((uint32_t)(buf)[0]<<24);
#endif

#ifdef _MSC_VER
#undef twp__ARITH_DEC_READ_HELPER
twp__INLINE twp__arith_dec_type twp__ARITH_DEC_READ_HELPER(uint8_t *buf)
{
    // msvc (and every other compiler) optimizes the endian check out and emits just a mov+bswap
    twp__arith_dec_type val;
    memcpy(&val, buf, sizeof(twp__arith_dec_type));
    uint16_t x = 0x1234;
    int is_little_endian = (*(uint8_t *)&x == 0x34);
    if (is_little_endian) {
#ifdef twp__64BIT
        val = _byteswap_uint64(val);
#else
        val = _byteswap_ulong(val);
#endif
    }
    return val;
}
#endif

typedef struct {
    uint8_t *data;
    int data_len;
    int data_at;
    twp__arith_dec_type value;
    twp__arith_dec_type range;
    int num_bits;
    int err;
} twp__arith_dec;

// leading 0 bits for a byte value, used in arithmetic decoding
static const uint8_t twp__lz_table[256] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    // rest is 0
};

typedef enum {
    twp__DCT_VAL_0,
    twp__DCT_VAL_1,
    twp__DCT_VAL_2,
    twp__DCT_VAL_3,
    twp__DCT_VAL_4,
    twp__DCT_RANGE_0,
    twp__DCT_RANGE_1,
    twp__DCT_RANGE_2,
    twp__DCT_RANGE_3,
    twp__DCT_RANGE_4,
    twp__DCT_RANGE_5,
    twp__DCT_EOB,
    twp__NUM_DCT_TOKENS
} twp__dct_token;
static const int twp__dct_token_tree[] = {
    -twp__DCT_EOB, 2,
        -twp__DCT_VAL_0, 4,
            -twp__DCT_VAL_1, 6,
                8, 12,
                    -twp__DCT_VAL_2, 10,
                        -twp__DCT_VAL_3, -twp__DCT_VAL_4,
                    14, 16,
                        -twp__DCT_RANGE_0, -twp__DCT_RANGE_1,
                    18, 20,
                        -twp__DCT_RANGE_2, -twp__DCT_RANGE_3,
                        -twp__DCT_RANGE_4, -twp__DCT_RANGE_5,
};
static const int twp__dct_range_probs[][12] = { // index with enum_val - twp__DCT_RANGE_0
    {159, 0},
    {165, 145, 0},
    {173, 148, 140, 0},
    {176, 155, 140, 135, 0},
    {180, 157, 141, 134, 130, 0},
    {254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0},
};
static const int twp__dct_range_base[] = {5, 7, 11, 19, 35, 67};

typedef enum {
    twp__DCT_TYPE_Y_WITHOUT_DC,
    twp__DCT_TYPE_Y2,
    twp__DCT_TYPE_UV,
    twp__DCT_TYPE_Y_WITH_DC,
    twp__NUM_DCT_TYPES
} twp__dct_type;

typedef enum {
    twp__DCT_PLANE_Y,
    twp__DCT_PLANE_Y2,
    twp__DCT_PLANE_U,
    twp__DCT_PLANE_V,
    twp__NUM_DCT_PLANES
} twp__dct_plane;

typedef enum {
    twp__DC,
    twp__AC,
    twp__NUM_DCT_COEFF_TYPES
} twp__dct_coeff_type;

#define twp__COEFF_BAND_MAX 7
static const int twp__coeff_bands[16] = {
    0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
};

#define twp__NUM_DCT_CTXS 3
typedef int twp__coeff_prob_type[twp__NUM_DCT_TYPES][twp__COEFF_BAND_MAX+1][twp__NUM_DCT_CTXS][twp__NUM_DCT_TOKENS-1];

static const twp__coeff_prob_type twp__coeff_update_probs = {
    {
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255},
            {249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255},
            {234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255},
            {250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255},
            {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        }
    },
    {
        {
            {217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255},
            {234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255},
        },
        {
            {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        }
    },
    {
        {
            {186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255},
            {234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255},
            {251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255},
        },
        {
            {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        }
    },
    {
        {
            {248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255},
            {248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255},
            {248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255},
            {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        },
        {
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
            {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
        }
    }
};

static const twp__coeff_prob_type twp__default_coeff_probs = {
    {
        {
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
        },
        {
            {253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
            {189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
            {106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128},
        },
        {
            {  1,  98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
            {181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
            { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128},
        },
        {
            {  1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
            {184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
            { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128},
        },
        {
            {  1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
            {170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
            { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128},
        },
        {
            {  1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
            {207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
            {102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128},
        },
        {
            {  1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
            {177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
            { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128},
        },
        {
            {  1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {246,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
        }
    },
    {
        {
            {198,  35, 237, 223, 193, 187, 162, 160, 145, 155,  62},
            {131,  45, 198, 221, 172, 176, 220, 157, 252, 221,   1},
            { 68,  47, 146, 208, 149, 167, 221, 162, 255, 223, 128},
        },
        {
            {  1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
            {184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
            { 81,  99, 181, 242, 176, 190, 249, 202, 255, 255, 128},
        },
        {
            {  1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
            { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
            { 23,  91, 163, 242, 170, 187, 247, 210, 255, 255, 128},
        },
        {
            {  1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
            {109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
            { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128},
        },
        {
            {  1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
            { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
            { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128},
        },
        {
            {  1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
            {124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
            { 35,  77, 181, 251, 193, 211, 255, 205, 128, 128, 128},
        },
        {
            {  1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
            {121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
            { 45,  99, 188, 251, 195, 217, 255, 224, 128, 128, 128},
        },
        {
            {  1,   1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
            {203,   1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
            {137,   1, 177, 255, 224, 255, 128, 128, 128, 128, 128},
        }
    },
    {
        {
            {253,   9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
            {175,  13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
            { 73,  17, 171, 221, 161, 179, 236, 167, 255, 234, 128},
        },
        {
            {  1,  95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
            {239,  90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
            {155,  77, 195, 248, 188, 195, 255, 255, 128, 128, 128},
        },
        {
            {  1,  24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
            {201,  51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
            { 69,  46, 190, 239, 201, 218, 255, 228, 128, 128, 128},
        },
        {
            {  1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
            {223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
            {141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128},
        },
        {
            {  1,  16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
            {190,  36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
            {149,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
        },
        {
            {  1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128},
        },
        {
            {  1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
            {213,  62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
            { 55,  93, 255, 128, 128, 128, 128, 128, 128, 128, 128},
        },
        {
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
            {128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
        }
    },
    {
        {
            {202,  24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
            {126,  38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
            { 61,  46, 138, 219, 151, 178, 240, 170, 255, 216, 128},
        },
        {
            {  1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
            {166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
            { 39,  77, 162, 232, 172, 180, 245, 178, 255, 255, 128},
        },
        {
            {  1,  52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
            {124,  74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
            { 24,  71, 130, 219, 154, 170, 243, 182, 255, 255, 128},
        },
        {
            {  1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
            {149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
            { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128},
        },
        {
            {  1,  81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
            {123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
            { 20,  95, 153, 243, 164, 173, 255, 203, 128, 128, 128},
        },
        {
            {  1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
            {168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
            { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128},
        },
        {
            {  1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
            {141,  84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
            { 42,  80, 160, 240, 162, 185, 255, 205, 128, 128, 128},
        },
        {
            {  1,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {244,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
            {238,   1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
        },
    },
};

typedef enum {
    twp__MB_MODE_DC_PRED,
    twp__MB_MODE_V_PRED,
    twp__MB_MODE_H_PRED,
    twp__MB_MODE_TM_PRED,
    twp__MB_MODE_B_PRED,

    twp__NUM_UV_MB_MODES = twp__MB_MODE_B_PRED,
    twp__NUM_Y_MB_MODES
} twp__mb_mode;
static const int twp__mb_y_mode_tree[] = {
    -twp__MB_MODE_B_PRED, 2,
        4, 6,
            -twp__MB_MODE_DC_PRED, -twp__MB_MODE_V_PRED,
            -twp__MB_MODE_H_PRED, -twp__MB_MODE_TM_PRED,
};
static const int twp__mb_y_mode_tree_probs[] = {145, 156, 163, 128};
static const int twp__mb_uv_mode_tree[] = {
    -twp__MB_MODE_DC_PRED, 2,
        -twp__MB_MODE_V_PRED, 4,
            -twp__MB_MODE_H_PRED, -twp__MB_MODE_TM_PRED,
};
static const int twp__mb_uv_mode_tree_probs[] = {142, 114, 183};

typedef enum {
    twp__SB_MODE_DC_PRED,
    twp__SB_MODE_TM_PRED,

    twp__SB_MODE_VE_PRED,
    twp__SB_MODE_HE_PRED,

    twp__SB_MODE_LD_PRED,
    twp__SB_MODE_RD_PRED,

    twp__SB_MODE_VR_PRED,
    twp__SB_MODE_VL_PRED,
    twp__SB_MODE_HD_PRED,
    twp__SB_MODE_HU_PRED,

    twp__NUM_SUBBLOCK_MODES
} twp__sb_mode;
static const int twp__sb_mode_tree[] = {
    -twp__SB_MODE_DC_PRED, 2,
        -twp__SB_MODE_TM_PRED, 4,
            -twp__SB_MODE_VE_PRED, 6,
            8, 12,
                -twp__SB_MODE_HE_PRED, 10,
                    -twp__SB_MODE_RD_PRED, -twp__SB_MODE_VR_PRED,
            -twp__SB_MODE_LD_PRED, 14,
                -twp__SB_MODE_VL_PRED, 16,
                    -twp__SB_MODE_HD_PRED, -twp__SB_MODE_HU_PRED,
};
static const int twp__sb_mode_tree_probs[twp__NUM_SUBBLOCK_MODES][twp__NUM_SUBBLOCK_MODES][twp__NUM_SUBBLOCK_MODES-1] = {
    {
        {231, 120,  48,  89, 115, 113, 120, 152, 112},
        {152, 179,  64, 126, 170, 118,  46,  70,  95},
        {175,  69, 143,  80,  85,  82,  72, 155, 103},
        { 56,  58,  10, 171, 218, 189,  17,  13, 152},
        {144,  71,  10,  38, 171, 213, 144,  34,  26},
        {114,  26,  17, 163,  44, 195,  21,  10, 173},
        {121,  24,  80, 195,  26,  62,  44,  64,  85},
        {170,  46,  55,  19, 136, 160,  33, 206,  71},
        { 63,  20,   8, 114, 114, 208,  12,   9, 226},
        { 81,  40,  11,  96, 182,  84,  29,  16,  36},
    },
    {
        {134, 183,  89, 137,  98, 101, 106, 165, 148},
        { 72, 187, 100, 130, 157, 111,  32,  75,  80},
        { 66, 102, 167,  99,  74,  62,  40, 234, 128},
        { 41,  53,   9, 178, 241, 141,  26,   8, 107},
        {104,  79,  12,  27, 217, 255,  87,  17,   7},
        { 74,  43,  26, 146,  73, 166,  49,  23, 157},
        { 65,  38, 105, 160,  51,  52,  31, 115, 128},
        { 87,  68,  71,  44, 114,  51,  15, 186,  23},
        { 47,  41,  14, 110, 182, 183,  21,  17, 194},
        { 66,  45,  25, 102, 197, 189,  23,  18,  22},
    },
    {
        { 88,  88, 147, 150,  42,  46,  45, 196, 205},
        { 43,  97, 183, 117,  85,  38,  35, 179,  61},
        { 39,  53, 200,  87,  26,  21,  43, 232, 171},
        { 56,  34,  51, 104, 114, 102,  29,  93,  77},
        {107,  54,  32,  26,  51,   1,  81,  43,  31},
        { 39,  28,  85, 171,  58, 165,  90,  98,  64},
        { 34,  22, 116, 206,  23,  34,  43, 166,  73},
        { 68,  25, 106,  22,  64, 171,  36, 225, 114},
        { 34,  19,  21, 102, 132, 188,  16,  76, 124},
        { 62,  18,  78,  95,  85,  57,  50,  48,  51},
    },
    {
        {193, 101,  35, 159, 215, 111,  89,  46, 111},
        { 60, 148,  31, 172, 219, 228,  21,  18, 111},
        {112, 113,  77,  85, 179, 255,  38, 120, 114},
        { 40,  42,   1, 196, 245, 209,  10,  25, 109},
        {100,  80,   8,  43, 154,   1,  51,  26,  71},
        { 88,  43,  29, 140, 166, 213,  37,  43, 154},
        { 61,  63,  30, 155,  67,  45,  68,   1, 209},
        {142,  78,  78,  16, 255, 128,  34, 197, 171},
        { 41,  40,   5, 102, 211, 183,   4,   1, 221},
        { 51,  50,  17, 168, 209, 192,  23,  25,  82},
    },
    {
        {125,  98,  42,  88, 104,  85, 117, 175,  82},
        { 95,  84,  53,  89, 128, 100, 113, 101,  45},
        { 75,  79, 123,  47,  51, 128,  81, 171,   1},
        { 57,  17,   5,  71, 102,  57,  53,  41,  49},
        {115,  21,   2,  10, 102, 255, 166,  23,   6},
        { 38,  33,  13, 121,  57,  73,  26,   1,  85},
        { 41,  10,  67, 138,  77, 110,  90,  47, 114},
        {101,  29,  16,  10,  85, 128, 101, 196,  26},
        { 57,  18,  10, 102, 102, 213,  34,  20,  43},
        {117,  20,  15,  36, 163, 128,  68,   1,  26},
    },
    {
        {138,  31,  36, 171,  27, 166,  38,  44, 229},
        { 67,  87,  58, 169,  82, 115,  26,  59, 179},
        { 63,  59,  90, 180,  59, 166,  93,  73, 154},
        { 40,  40,  21, 116, 143, 209,  34,  39, 175},
        { 57,  46,  22,  24, 128,   1,  54,  17,  37},
        { 47,  15,  16, 183,  34, 223,  49,  45, 183},
        { 46,  17,  33, 183,   6,  98,  15,  32, 183},
        { 65,  32,  73, 115,  28, 128,  23, 128, 205},
        { 40,   3,   9, 115,  51, 192,  18,   6, 223},
        { 87,  37,   9, 115,  59,  77,  64,  21,  47},
    },
    {
        {104,  55,  44, 218,   9,  54,  53, 130, 226},
        { 64,  90,  70, 205,  40,  41,  23,  26,  57},
        { 54,  57, 112, 184,   5,  41,  38, 166, 213},
        { 30,  34,  26, 133, 152, 116,  10,  32, 134},
        { 75,  32,  12,  51, 192, 255, 160,  43,  51},
        { 39,  19,  53, 221,  26, 114,  32,  73, 255},
        { 31,   9,  65, 234,   2,  15,   1, 118,  73},
        { 88,  31,  35,  67, 102,  85,  55, 186,  85},
        { 56,  21,  23, 111,  59, 205,  45,  37, 192},
        { 55,  38,  70, 124,  73, 102,   1,  34,  98},
    },
    {
        {102,  61,  71,  37,  34,  53,  31, 243, 192},
        { 69,  60,  71,  38,  73, 119,  28, 222,  37},
        { 68,  45, 128,  34,   1,  47,  11, 245, 171},
        { 62,  17,  19,  70, 146,  85,  55,  62,  70},
        { 75,  15,   9,   9,  64, 255, 184, 119,  16},
        { 37,  43,  37, 154, 100, 163,  85, 160,   1},
        { 63,   9,  92, 136,  28,  64,  32, 201,  85},
        { 86,   6,  28,   5,  64, 255,  25, 248,   1},
        { 56,   8,  17, 132, 137, 255,  55, 116, 128},
        { 58,  15,  20,  82, 135,  57,  26, 121,  40},
    },
    {
        {164,  50,  31, 137, 154, 133,  25,  35, 218},
        { 51, 103,  44, 131, 131, 123,  31,   6, 158},
        { 86,  40,  64, 135, 148, 224,  45, 183, 128},
        { 22,  26,  17, 131, 240, 154,  14,   1, 209},
        { 83,  12,  13,  54, 192, 255,  68,  47,  28},
        { 45,  16,  21,  91,  64, 222,   7,   1, 197},
        { 56,  21,  39, 155,  60, 138,  23, 102, 213},
        { 85,  26,  85,  85, 128, 128,  32, 146, 171},
        { 18,  11,   7,  63, 144, 171,   4,   4, 246},
        { 35,  27,  10, 146, 174, 171,  12,  26, 128},
    },
    {
        {190,  80,  35,  99, 180,  80, 126,  54,  45},
        { 85, 126,  47,  87, 176,  51,  41,  20,  32},
        {101,  75, 128, 139, 118, 146, 116, 128,  85},
        { 56,  41,  15, 176, 236,  85,  37,   9,  62},
        {146,  36,  19,  30, 171, 255,  97,  27,  20},
        { 71,  30,  17, 119, 118, 255,  17,  18, 138},
        {101,  38,  60, 138,  55,  70,  43,  26, 142},
        {138,  45,  61,  62, 219,   1,  81, 188,  64},
        { 32,  41,  20, 117, 151, 142,  20,  21, 163},
        {112,  19,  12,  61, 195, 128,  48,   4,  24},
    },
};

static const int twp__segment_id_tree[] = {
    2,  4,
        -0, -1,
            -2, -3,
};

#define twp__QUANT_TABLE_SIZE 128

static const int twp__quant_tables[twp__NUM_DCT_COEFF_TYPES][twp__QUANT_TABLE_SIZE] = {
    { // dc
        4,   5,   6,   7,   8,   9,  10,  10,   11,  12,  13,  14,  15,
        16,  17,  17,  18,  19,  20,  20,  21,   21,  22,  22,  23,  23,
        24,  25,  25,  26,  27,  28,  29,  30,   31,  32,  33,  34,  35,
        36,  37,  37,  38,  39,  40,  41,  42,   43,  44,  45,  46,  46,
        47,  48,  49,  50,  51,  52,  53,  54,   55,  56,  57,  58,  59,
        60,  61,  62,  63,  64,  65,  66,  67,   68,  69,  70,  71,  72,
        73,  74,  75,  76,  76,  77,  78,  79,   80,  81,  82,  83,  84,
        85,  86,  87,  88,  89,  91,  93,  95,   96,  98, 100, 101, 102,
        104, 106, 108, 110, 112, 114, 116, 118, 122, 124, 126, 128, 130,
        132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,
    },
    { // ac
        4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,
        17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
        30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
        43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
        56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,  78,
        80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104,
       106, 108, 110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137,
       140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173, 177,
       181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229,
       234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,
    },
};

static const int twp__zigzag[] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

#define twp__MAX_PARITIONS 8

typedef struct {
    int offset;
    int size;
    twp__arith_dec dec;
} twp__dct_partition;

#define twp__MAX_SEGMENTS 4

typedef enum {
    twp__SEGMENT_MODE_DELTA,
    twp__SEGMENT_MODE_ABSOLUTE
} twp__segment_mode;

typedef struct {
    int quant_update;
    int lf_update;
} twp__segment;

typedef struct {
    // this is a relatively big allocation, so try to keep it reasonably sized
    uint8_t skip;
    uint8_t segment_id;
    uint8_t y_mode;          // twp__mb_mode
    uint8_t y_sb_modes[16];  // twp__sb_mode
    uint8_t uv_mode;         // twp__mb_mode
    uint8_t skip_sb_filtering;
    int x;
    int y;
} twp__mb_info;

typedef enum {
    twp__FILTER_NORMAL,
    twp__FILTER_SIMPLE
} twp__filter_type;

typedef enum {
    twp__UPSCALING_NONE,
    twp__UPSCALING_5_OVER_4,
    twp__UPSCALING_5_OVER_3,
    twp__UPSCALING_2
} twp__upscaling;

typedef struct {
    int width;
    int height;
    int luma_width;
    int luma_height;
    int luma_stride;
    int chroma_width;
    int chroma_height;
    int chroma_stride;

    twp__upscaling upscaling_x;
    twp__upscaling upscaling_y;

    twp__mb_info *mb_infos;
    int num_mbs;
    int mbs_per_row;
    int mbs_per_col;
    int num_sbs;
    int sbs_per_row;
    int sbs_per_col;

    int require_clamping;

    twp__filter_type loop_filter_type;
    int loop_filter_level;
    int loop_filter_sharpness;

    int lf_adj_enabled;
    int lf_adj_ref_frame[4];
    int lf_adj_mb_mode[4];

    int frame_quant_idx_base;
    int quant_idx_deltas[twp__NUM_DCT_PLANES][twp__NUM_DCT_COEFF_TYPES];

    int segmentation_enabled;
    int all_mbs_are_segment_0;
    twp__segment_mode segment_mode;
    twp__segment segments[twp__MAX_SEGMENTS];
    int segment_id_tree_probs[3];

    int first_partition_size;
    twp__arith_dec first_partition_dec;

    int num_dct_partitions;
    twp__dct_partition dct_partitions[twp__MAX_PARITIONS];

    twp__coeff_prob_type coeff_probs;

    uint8_t *plane_y;
    uint8_t *plane_u;
    uint8_t *plane_v;
} twp__vp8_data;

#define twp__UNCOMPRESSED_VP8_HEADER_SIZE 10

static void twp__init_arith_decoder(twp__arith_dec *dec, uint8_t *data, int data_len)
{
    twp__assert(data_len >= 1);

    dec->data = data;
    dec->data_len = data_len;
    dec->data_at = 1;
    dec->value = (twp__arith_dec_type)data[0] << twp__ARITH_DEC_SHIFT;
    dec->range = 255;
    dec->num_bits = 0;
    dec->err = 0;
}

twp__INLINE static int twp__read_arith_bit(twp__arith_dec *dec, int prob)
{
    uint8_t shift = twp__lz_table[dec->range];
    dec->range <<= shift;
    dec->value <<= shift;
    dec->num_bits -= shift;
    if (dec->num_bits < 0) {
        int bytes_left = dec->data_len - dec->data_at;
        if (bytes_left >= (int)sizeof(twp__arith_dec_type)) {
            twp__arith_dec_type val = twp__ARITH_DEC_READ_HELPER(dec->data + dec->data_at);
            dec->value |= val >> (8 + dec->num_bits);
            dec->num_bits += twp__ARITH_DEC_SHIFT;
            dec->data_at += sizeof(twp__arith_dec_type)-1;
        } else if (bytes_left >= 1) {
            for (int i = 0; i < bytes_left; ++i)
                dec->value |= (twp__arith_dec_type)dec->data[dec->data_at++] << ((twp__ARITH_DEC_SHIFT-8 - i*8) - dec->num_bits);
            dec->num_bits += bytes_left * 8;
        } else {
            dec->err = 1;
        }
    }

    twp__arith_dec_type split = 1 + (((dec->range - 1) * (twp__arith_dec_type)prob) >> 8);
    int result;
    if (dec->value < (split << twp__ARITH_DEC_SHIFT)) {
        dec->range = split;
        result = 0;
    } else {
        dec->range -= split;
        dec->value -= split << twp__ARITH_DEC_SHIFT;
        result = 1;
    }

    return result;
}

twp__INLINE static int twp__read_arith_literal(twp__arith_dec *dec, int n)
{
    twp__assert(n >= 1 && n <= 31);
    int result = 0;
    for (int i = 0; i < n; ++i) {
        int bit = twp__read_arith_bit(dec, 128);
        result <<= 1;
        result |= bit;
    }
    return result;
}

twp__INLINE static int twp__read_arith_signed_literal(twp__arith_dec *dec, int n)
{
    twp__assert(n >= 1 && n <= 31);
    int lit = twp__read_arith_literal(dec, n);
    int sign = twp__read_arith_literal(dec, 1);
    return sign ? -lit : lit;
}

twp__INLINE static int twp__read_arith_flagged_signed_literal(twp__arith_dec *dec, int n, int value_if_flag_is_zero)
{
    int flag = twp__read_arith_literal(dec, 1);
    if (flag)
        return twp__read_arith_signed_literal(dec, n);
    else
        return value_if_flag_is_zero;
}

twp__INLINE static int twp__read_arith_tree(twp__arith_dec *dec, const int *tree, const int *prob, int init)
{
    // note that assuming that tree and prob are always valid (which they should be),
    // we never have to check if i and i >> 1 are in bounds
    int i = init;
    do {
        int p = prob[i >> 1];
        int b = twp__read_arith_bit(dec, p);
        i = tree[i + b];
    } while (i > 0);
    return -i;
}

twp__INLINE static int twp__get_quant(twp__dct_coeff_type type, int idx)
{
    if      (idx < 0)                      idx = 0;
    else if (idx >= twp__QUANT_TABLE_SIZE) idx = twp__QUANT_TABLE_SIZE-1;
    return twp__quant_tables[type][idx];
}

static int twp__read_residual_block(twp__arith_dec *dec, twp__dct_type dct_type, short *coeffs,
        twp__coeff_prob_type coeff_probs, uint8_t *nz_left, uint8_t *nz_above, int dc_quant, int ac_quant)
{
    memset(coeffs, 0, sizeof(*coeffs) * 16);

    int all_zero = 1;

    int ctx = 0;
    if (*nz_left) ++ctx;
    if (*nz_above) ++ctx;
    *nz_left = 0;
    *nz_above = 0;

    for (int i = (dct_type == twp__DCT_TYPE_Y_WITHOUT_DC); i < 16; ++i) {
        twp__assert(ctx >= 0 && ctx <= 2);

        int *prob = coeff_probs[dct_type][twp__coeff_bands[i]][ctx];

        if (twp__read_arith_bit(dec, prob[0]) == 0)
            break; // eob

        // this works because if we just decoded a 0, the first level of the tree (which is eob) is skipped.
        // this is because it doesn't make any sense to decode an eob right after a 0. you should have
        // just decoded an eob instead.
        while (twp__read_arith_bit(dec, prob[1]) == 0) {
            ctx = 0;
            ++i;
            if (i == 16)
                return all_zero;
            prob = coeff_probs[dct_type][twp__coeff_bands[i]][ctx];
        }

        int coeff = twp__read_arith_tree(dec, twp__dct_token_tree, prob, 4);

        if (coeff >= twp__DCT_RANGE_0) {
            const int *range_prob = twp__dct_range_probs[coeff - twp__DCT_RANGE_0];
            int base = twp__dct_range_base[coeff - twp__DCT_RANGE_0];
            coeff = 0;
            int j = 0;
            for (;;) {
                int p = range_prob[j++];
                if (p == 0)
                    break;
                int bit = twp__read_arith_bit(dec, p);
                coeff += coeff + bit;
            }
            coeff += base;
        }

        int sign = twp__read_arith_literal(dec, 1);
        if (sign)
            coeff = -coeff;

        *nz_left = 1;
        *nz_above = 1;
        all_zero = 0;

        coeffs[twp__zigzag[i]] = (short)(coeff * ((i == 0) ? dc_quant : ac_quant));

        ctx = coeff;
        if (ctx < 0) ctx = -ctx;
        if (ctx > 2) ctx = 2;
    }

    return all_zero;
}

#ifdef twp__SSE2

twp__INLINE static __m128i twp__idct_C(__m128i x)
{
    // x * (sqrt(2) * cos(pi/8))
    // the same reasoning as int the c version of the function applies, except here we really don't even
    // have a choice because we use mulhi_epi16, so x must be < 1
    return _mm_add_epi16(x, _mm_mulhi_epi16(x, _mm_set1_epi16(20091)));
}

twp__INLINE static __m128i twp__idct_S(__m128i x)
{
    // x * (sqrt(2) * sin(pi/8))
    // in this case x is < 1, but the resulting fixed-point value does not fit into a
    // signed 16-bit number, so we use the same trick as above
    return _mm_add_epi16(x, _mm_mulhi_epi16(x, _mm_set1_epi16(-30068)));
}

twp__INLINE static void twp__idct_step(__m128i *i0, __m128i *i1, __m128i *i2, __m128i *i3)
{
    __m128i t0 = _mm_add_epi16(*i0, *i2);
    __m128i t1 = _mm_sub_epi16(*i0, *i2);
    __m128i t2 = _mm_sub_epi16(twp__idct_S(*i1), twp__idct_C(*i3));
    __m128i t3 = _mm_add_epi16(twp__idct_C(*i1), twp__idct_S(*i3));
    *i0 = _mm_add_epi16(t0, t3);
    *i1 = _mm_add_epi16(t1, t2);
    *i2 = _mm_sub_epi16(t1, t2);
    *i3 = _mm_sub_epi16(t0, t3);
}

twp__INLINE static void twp__idct_transpose(__m128i *i0, __m128i *i1, __m128i *i2, __m128i *i3)
{
    // i0 =                                        0  1  2  3 .. .. .. ..
    // i1 =                                        4  5  6  7 .. .. .. ..
    // i2 =                                        8  9 10 11 .. .. .. ..
    // i3 =                                       12 13 14 15 .. .. .. ..

    __m128i t0 = _mm_unpacklo_epi16(*i0, *i1); //  0  4  1  5  2  6  3  7
    __m128i t1 = _mm_unpacklo_epi16(*i2, *i3); //  8 12  9 13 10 14 11 15

    *i0 = _mm_unpacklo_epi32(t0, t1);          //  0  4  8 12  1  5  9 13
    *i1 = _mm_unpackhi_epi64(*i0, *i0);        //  1  5  9 13  1  5  9 13
    *i2 = _mm_unpackhi_epi32(t0, t1);          //  2  6 10 14  3  7 11 15
    *i3 = _mm_unpackhi_epi64(*i2, *i2);        //  3  7 11 15  3  7 11 15
}

twp__INLINE static void twp__idct_write(__m128i xmm, uint8_t *plane_buf, int sb_offset, int stride, int n)
{
    __m128i tmp;
    int ub_sucks;
    memcpy(&ub_sucks, plane_buf + sb_offset + stride*n, 4);
    tmp = _mm_cvtsi32_si128(ub_sucks);
    tmp = _mm_unpacklo_epi8(tmp, _mm_setzero_si128());
    tmp = _mm_add_epi16(tmp, xmm);
    tmp = _mm_packus_epi16(tmp, tmp); // this does the clamping for free
    ub_sucks = _mm_cvtsi128_si32(tmp);
    memcpy(plane_buf + sb_offset + stride*n, &ub_sucks, 4);
}

#else

twp__INLINE static int twp__idct_C(int x)
{
    // x * (sqrt(2) * cos(pi/8))
    // because sqrt(2) * cos(pi/8) > 1, the fixed-point math can overflow,
    // so we use the fact that x*a = x*(a-1) + x.
    return (x + twp__sra(x * 20091, 16));
}

twp__INLINE static int twp__idct_S(int x)
{
    // x * (sqrt(2) * sin(pi/8))
    return twp__sra(x * 35468, 16);
}

#endif

static void twp__idct(short *input, uint8_t *plane_buf, int mb_offset, int sb_x, int sb_y, int stride)
{
    // the prediction must already be written into plane_buf when this function is called!

    int sb_offset = mb_offset + sb_y*4*stride + sb_x*4;

#ifndef twp__SSE2
    for (int i = 0; i < 4; i++) {
        short *col = input + i;
        int t0 = col[0*4] + col[2*4];
        int t1 = col[0*4] - col[2*4];
        int t2 = twp__idct_S(col[1*4]) - twp__idct_C(col[3*4]);
        int t3 = twp__idct_C(col[1*4]) + twp__idct_S(col[3*4]);
        col[0*4] = (short)(t0 + t3);
        col[1*4] = (short)(t1 + t2);
        col[2*4] = (short)(t1 - t2);
        col[3*4] = (short)(t0 - t3);
    }

    for (int i = 0; i < 4; i++) {
        short *row = input + i*4;
        int t0 = row[0] + row[2];
        int t1 = row[0] - row[2];
        int t2 = twp__idct_S(row[1]) - twp__idct_C(row[3]);
        int t3 = twp__idct_C(row[1]) + twp__idct_S(row[3]);
        row[0] = (short)(twp__sra(t0 + t3 + 4, 3));
        row[1] = (short)(twp__sra(t1 + t2 + 4, 3));
        row[2] = (short)(twp__sra(t1 - t2 + 4, 3));
        row[3] = (short)(twp__sra(t0 - t3 + 4, 3));
    }

    short *src_ptr = input;
    uint8_t *dst_ptr = plane_buf + sb_offset;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            int val = dst_ptr[j] + src_ptr[j];
            dst_ptr[j] = (uint8_t)twp__clamp(val, 0, 255);
        }
        src_ptr += 4;
        dst_ptr += stride;
    }
#else
    // the biggest coefficient is +-2048, which means we never have to worry about 16-bit overflow here

    __m128i i0, i1, i2, i3;

    i0 = _mm_loadl_epi64((__m128i *)(input + 0));
    i1 = _mm_loadl_epi64((__m128i *)(input + 4));
    i2 = _mm_loadl_epi64((__m128i *)(input + 8));
    i3 = _mm_loadl_epi64((__m128i *)(input + 12));

    twp__idct_step(&i0, &i1, &i2, &i3);
    twp__idct_transpose(&i0, &i1, &i2, &i3);
    twp__idct_step(&i0, &i1, &i2, &i3);

    i0 = _mm_srai_epi16(_mm_add_epi16(i0, _mm_set1_epi16(4)), 3);
    i1 = _mm_srai_epi16(_mm_add_epi16(i1, _mm_set1_epi16(4)), 3);
    i2 = _mm_srai_epi16(_mm_add_epi16(i2, _mm_set1_epi16(4)), 3);
    i3 = _mm_srai_epi16(_mm_add_epi16(i3, _mm_set1_epi16(4)), 3);

    twp__idct_transpose(&i0, &i1, &i2, &i3);

    twp__idct_write(i0, plane_buf, sb_offset, stride, 0);
    twp__idct_write(i1, plane_buf, sb_offset, stride, 1);
    twp__idct_write(i2, plane_buf, sb_offset, stride, 2);
    twp__idct_write(i3, plane_buf, sb_offset, stride, 3);
#endif
}

static void twp__iwht(short *input)
{
    // this function should probably be simd'ized, but it doesn't seem to be a hotspot,
    // so i didn't bother (yet)..

    short *ip = input;
    short *op = input;
    for (int i = 0; i < 4; i++) {
        int a1 = ip[0] + ip[12];
        int b1 = ip[4] + ip[8];
        int c1 = ip[4] - ip[8];
        int d1 = ip[0] - ip[12];
        op[0] = (short)(a1 + b1);
        op[4] = (short)(c1 + d1);
        op[8] = (short)(a1 - b1);
        op[12]= (short)(d1 - c1);
        ip++;
        op++;
    }
    ip = input;
    op = input;
    for (int i = 0;i < 4; i++) {
        int a1 = ip[0] + ip[3];
        int b1 = ip[1] + ip[2];
        int c1 = ip[1] - ip[2];
        int d1 = ip[0] - ip[3];
        int a2 = a1 + b1;
        int b2 = c1 + d1;
        int c2 = a1 - b1;
        int d2 = d1 - c1;
        op[0] = (short)twp__sra(a2+3, 3);
        op[1] = (short)twp__sra(b2+3, 3);
        op[2] = (short)twp__sra(c2+3, 3);
        op[3] = (short)twp__sra(d2+3, 3);
        ip += 4;
        op += 4;
    }
}

twp__INLINE static uint8_t twp__avg2_vp8(uint8_t a, uint8_t b)
{
    return (uint8_t)((a + b + 1) >> 1);
}

twp__INLINE static uint8_t twp__weighted_avg3(uint8_t a, uint8_t b, uint8_t c)
{
    return (uint8_t)((a + b + b + c + 2) >> 2);
}

static void twp__predict_luma_subblock(uint8_t *buf, twp__mb_info *mb_info, int sx, int sy, int mb_offset, int stride)
{
    twp__assert(mb_info->y_mode == twp__MB_MODE_B_PRED);

    int sb_mode = mb_info->y_sb_modes[sy*4 + sx];
    int sb_offset = mb_offset + sy*4*stride + sx*4;

    uint8_t left[4];
    for (int i = 0; i < 4; ++i)
        left[i] = buf[sb_offset + i*stride - 1];
    uint8_t *above = buf + sb_offset - stride;
    uint8_t p = above[-1];

    uint8_t *dst = buf + sb_offset;

    switch (sb_mode) {
        case twp__SB_MODE_DC_PRED: {
            // this isn't really explained in the spec, and considering how the 16x16 version of DC_PRED works,
            // it's kind of criminal to not explicitly state that they differ completely

#ifndef twp__SSE2
            int avg = 0;
            for (int i = 0; i < 4; ++i) {
                avg += left[i];
                avg += above[i];
            }
            avg = (avg + 4) >> 3;
            twp__assert(avg >= 0 && avg <= 255);
            uint8_t avg8 = (uint8_t)avg;
            memset(dst, avg8, 4); dst += stride;
            memset(dst, avg8, 4); dst += stride;
            memset(dst, avg8, 4); dst += stride;
            memset(dst, avg8, 4); dst += stride;
#else
            int i0;
            int i1;
            memcpy(&i0, left, 4);
            memcpy(&i1, above, 4);
            __m128i l = _mm_cvtsi32_si128(i0);
            __m128i a = _mm_cvtsi32_si128(i1);
            __m128i joined = _mm_unpacklo_epi32(l, a);
            __m128i sum = _mm_sad_epu8(joined, _mm_setzero_si128());
            __m128i avg = _mm_srli_epi16(_mm_add_epi16(sum, _mm_set1_epi16(4)), 3);
            __m128i avg2 = _mm_unpacklo_epi8(avg, avg);
            __m128i avg4 = _mm_unpacklo_epi8(avg2, avg2);
            int res = _mm_cvtsi128_si32(avg4);
            memcpy(dst, &res, 4); dst += stride;
            memcpy(dst, &res, 4); dst += stride;
            memcpy(dst, &res, 4); dst += stride;
            memcpy(dst, &res, 4); dst += stride;
#endif
        } break;

        case twp__SB_MODE_TM_PRED: {
#ifndef twp__SSE2
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    int val = left[y] + above[x] - p;
                    if (val < 0) val = 0;
                    if (val > 255) val = 255;
                    dst[x] = (uint8_t)val;
                }
                dst += stride;
            }
#else
            int i;
            memcpy(&i, above, 4);
            __m128i a = _mm_cvtsi32_si128(i);
            a = _mm_unpacklo_epi8(a, _mm_setzero_si128());
            __m128i p_ = _mm_set1_epi16(p);

            for (i = 0; i < 4; ++i) {
                __m128i l = _mm_set1_epi16(left[i]);
                __m128i row = _mm_sub_epi16(_mm_add_epi16(a, l), p_);
                row = _mm_packus_epi16(row, row);
                int res = _mm_cvtsi128_si32(row);
                memcpy(dst, &res, 4);
                dst += stride;
            }
#endif
        } break;

        case twp__SB_MODE_VE_PRED: {
            uint8_t row[4];
            for (int x = 0; x < 4; ++x) {
                uint8_t avg = twp__weighted_avg3(above[x-1], above[x], above[x+1]);
                row[x] = avg;
            }
            memcpy(dst, row, 4); dst += stride;
            memcpy(dst, row, 4); dst += stride;
            memcpy(dst, row, 4); dst += stride;
            memcpy(dst, row, 4); dst += stride;
        } break;

        case twp__SB_MODE_HE_PRED: {
            memset(dst, twp__weighted_avg3(p, left[0], left[1]), 4); dst += stride;
            memset(dst, twp__weighted_avg3(left[0], left[1], left[2]), 4); dst += stride;
            memset(dst, twp__weighted_avg3(left[1], left[2], left[3]), 4); dst += stride;
            memset(dst, twp__weighted_avg3(left[2], left[3], left[3]), 4); dst += stride;
        } break;

        case twp__SB_MODE_LD_PRED: {
            dst[0*stride+0] = twp__weighted_avg3(above[0], above[1], above[2]);
            dst[0*stride+1] = twp__weighted_avg3(above[1], above[2], above[3]);
            dst[1*stride+0] = dst[0*stride+1];
            dst[0*stride+2] = twp__weighted_avg3(above[2], above[3], above[4]);
            dst[1*stride+1] = dst[0*stride+2];
            dst[2*stride+0] = dst[0*stride+2];
            dst[0*stride+3] = twp__weighted_avg3(above[3], above[4], above[5]);
            dst[1*stride+2] = dst[0*stride+3];
            dst[2*stride+1] = dst[0*stride+3];
            dst[3*stride+0] = dst[0*stride+3];
            dst[1*stride+3] = twp__weighted_avg3(above[4], above[5], above[6]);
            dst[2*stride+2] = dst[1*stride+3];
            dst[3*stride+1] = dst[1*stride+3];
            dst[2*stride+3] = twp__weighted_avg3(above[5], above[6], above[7]);
            dst[3*stride+2] = dst[2*stride+3];
            dst[3*stride+3] = twp__weighted_avg3(above[6], above[7], above[7]);
        } break;

        case twp__SB_MODE_RD_PRED: {
            dst[stride*3+0] = twp__weighted_avg3(left[3], left[2], left[1]);
            dst[stride*3+1] = twp__weighted_avg3(left[2], left[1], left[0]);
            dst[stride*2+0] = dst[stride*3+1];
            dst[stride*3+2] = twp__weighted_avg3(left[1], left[0], p);
            dst[stride*2+1] = dst[stride*3+2];
            dst[stride*1+0] = dst[stride*3+2];
            dst[stride*3+3] = twp__weighted_avg3(left[0], p, above[0]);
            dst[stride*2+2] = dst[stride*3+3];
            dst[stride*1+1] = dst[stride*3+3];
            dst[stride*0+0] = dst[stride*3+3];
            dst[stride*2+3] = twp__weighted_avg3(p, above[0], above[1]);
            dst[stride*1+2] = dst[stride*2+3];
            dst[stride*0+1] = dst[stride*2+3];
            dst[stride*1+3] = twp__weighted_avg3(above[0], above[1], above[2]);
            dst[stride*0+2] = dst[stride*1+3];
            dst[stride*0+3] = twp__weighted_avg3(above[1], above[2], above[3]);
        } break;

        case twp__SB_MODE_VR_PRED: {
            dst[stride*3+0] = twp__weighted_avg3(left[2], left[1], left[0]);
            dst[stride*2+0] = twp__weighted_avg3(left[1], left[0], p);
            dst[stride*3+1] = twp__weighted_avg3(left[0], p, above[0]);
            dst[stride*1+0] = dst[stride*3+1];
            dst[stride*2+1] = twp__avg2_vp8(p, above[0]);
            dst[stride*0+0] = dst[stride*2+1];
            dst[stride*3+2] = twp__weighted_avg3(p, above[0], above[1]);
            dst[stride*1+1] = dst[stride*3+2];
            dst[stride*2+2] = twp__avg2_vp8(above[0], above[1]);
            dst[stride*0+1] = dst[stride*2+2];
            dst[stride*3+3] = twp__weighted_avg3(above[0], above[1], above[2]);
            dst[stride*1+2] = dst[stride*3+3];
            dst[stride*2+3] = twp__avg2_vp8(above[1], above[2]);
            dst[stride*0+2] = dst[stride*2+3];
            dst[stride*1+3] = twp__weighted_avg3(above[1], above[2], above[3]);
            dst[stride*0+3] = twp__avg2_vp8(above[2], above[3]);
        } break;

        case twp__SB_MODE_VL_PRED: {
            dst[stride*0+0] = twp__avg2_vp8(above[0], above[1]);
            dst[stride*1+0] = twp__weighted_avg3(above[0], above[1], above[2]);
            dst[stride*2+0] = twp__avg2_vp8(above[1], above[2]);
            dst[stride*0+1] = dst[stride*2+0];
            dst[stride*1+1] = twp__weighted_avg3(above[1], above[2], above[3]);
            dst[stride*3+0] = dst[stride*1+1];
            dst[stride*2+1] = twp__avg2_vp8(above[2], above[3]);
            dst[stride*0+2] = dst[stride*2+1];
            dst[stride*3+1] = twp__weighted_avg3(above[2], above[3], above[4]);
            dst[stride*1+2] = dst[stride*3+1];
            dst[stride*2+2] = twp__avg2_vp8(above[3], above[4]);
            dst[stride*0+3] = dst[stride*2+2];
            dst[stride*3+2] = twp__weighted_avg3(above[3], above[4], above[5]);
            dst[stride*1+3] = dst[stride*3+2];
            dst[stride*2+3] = twp__weighted_avg3(above[4], above[5], above[6]);
            dst[stride*3+3] = twp__weighted_avg3(above[5], above[6], above[7]);
        } break;

        case twp__SB_MODE_HD_PRED: {
            dst[stride*3+0] = twp__avg2_vp8(left[3], left[2]);
            dst[stride*3+1] = twp__weighted_avg3(left[3], left[2], left[1]);
            dst[stride*2+0] = twp__avg2_vp8(left[2], left[1]);
            dst[stride*3+2] = dst[stride*2+0];
            dst[stride*2+1] = twp__weighted_avg3(left[2], left[1], left[0]);
            dst[stride*3+3] = dst[stride*2+1];
            dst[stride*2+2] = twp__avg2_vp8(left[1], left[0]);
            dst[stride*1+0] = dst[stride*2+2];
            dst[stride*2+3] = twp__weighted_avg3(left[1], left[0], p);
            dst[stride*1+1] = dst[stride*2+3];
            dst[stride*1+2] = twp__avg2_vp8(left[0], p);
            dst[stride*0+0] = dst[stride*1+2];
            dst[stride*1+3] = twp__weighted_avg3(left[0], p, above[0]);
            dst[stride*0+1] = dst[stride*1+3];
            dst[stride*0+2] = twp__weighted_avg3(p, above[0], above[1]);
            dst[stride*0+3] = twp__weighted_avg3(above[0], above[1], above[2]);
        } break;

        case twp__SB_MODE_HU_PRED: {
            dst[stride*0+0] = twp__avg2_vp8(left[0], left[1]);
            dst[stride*0+1] = twp__weighted_avg3(left[0], left[1], left[2]);
            dst[stride*0+2] = twp__avg2_vp8(left[1], left[2]);
            dst[stride*1+0] = dst[stride*0+2];
            dst[stride*0+3] = twp__weighted_avg3(left[1], left[2], left[3]);
            dst[stride*1+1] = dst[stride*0+3];
            dst[stride*1+2] = twp__avg2_vp8(left[2], left[3]);
            dst[stride*2+0] = dst[stride*1+2];
            dst[stride*1+3] = twp__weighted_avg3(left[2], left[3], left[3]);
            dst[stride*2+1] = dst[stride*1+3];
            dst[stride*2+2] = left[3];
            dst[stride*2+3] = left[3];
            dst[stride*3+0] = left[3];
            dst[stride*3+1] = left[3];
            dst[stride*3+2] = left[3];
            dst[stride*3+3] = left[3];
        } break;

        default: {
            twp__assert(0);
        } break;
    }
}

static void twp__predict_macroblock(uint8_t *buf, twp__mb_info *mb_info, int chroma, int mb_offset, int stride)
{
    int block_size = chroma ? 8 : 16;
    int mode = chroma ? mb_info->uv_mode : mb_info->y_mode;

    uint8_t left[16];
    for (int i = 0; i < block_size; ++i)
        left[i] = buf[mb_offset + i*stride - 1];
    uint8_t *above = buf + mb_offset - stride;
    uint8_t p = above[-1];

    uint8_t *dst = buf + mb_offset;

    switch (mode) {
        case twp__MB_MODE_DC_PRED: {
            int avg = 0;
            if (mb_info->x == 0 && mb_info->y == 0) {
                avg = 128;
            } else if (mb_info->y == 0) {
                for (int i = 0; i < block_size; ++i) {
                    avg += left[i];
                }
                avg = (avg + (block_size/2)) / block_size;
            } else if (mb_info->x == 0) {
                for (int i = 0; i < block_size; ++i) {
                    avg += above[i];
                }
                avg = (avg + (block_size/2)) / block_size;
            } else {
                for (int i = 0; i < block_size; ++i) {
                    avg += above[i];
                    avg += left[i];
                }
                avg = (avg + block_size) / (block_size*2);
            }
            twp__assert(avg >= 0 && avg <= 255);

            uint8_t avg8 = (uint8_t)avg;
            for (int i = 0; i < block_size; ++i) {
                memset(dst, avg8, block_size);
                dst += stride;
            }
        } break;

        case twp__MB_MODE_V_PRED: {
            for (int i = 0; i < block_size; ++i) {
                memcpy(dst, above, block_size);
                dst += stride;
            }
        } break;

        case twp__MB_MODE_H_PRED: {
            for (int i = 0; i < block_size; ++i) {
                memset(dst, left[i], block_size);
                dst += stride;
            }
        } break;

        case twp__MB_MODE_TM_PRED: {
            for (int y = 0; y < block_size; ++y) {
                for (int x = 0; x < block_size; ++x) {
                    int val = left[y] + above[x] - p;
                    val = twp__clamp(val, 0, 255);
                    dst[x] = (uint8_t)val;
                }
                dst += stride;
            }
        } break;

        case twp__MB_MODE_B_PRED: {
            // this case must be handled differently, because the prediction process of the 4x4 subblocks
            // must be interleaved the with residual decoding of the 4x4 subblocks
            // (and for chroma this doesn't exist anyway)
            twp__assert(0);
        } break;

        default: {
            twp__assert(0);
        } break;
    }
}

static int twp__read_vp8_header(twp__vp8_data *data, uint8_t *raw_bytes, int num_bytes)
{
    if (num_bytes < twp__UNCOMPRESSED_VP8_HEADER_SIZE) return 0;

    int frame_type = raw_bytes[0] & 0x1;
    if (frame_type != 0) return 0; // 0 == keyframe, 1 == interframes, since we are decoding webp we only accept keyframes

    int show_frame = (raw_bytes[0] >> 4) & 0x1;
    if (show_frame != 1) return 0; // i assume this should always be 1 for keyframes?

    data->first_partition_size = (((int)raw_bytes[0] >> 5) & 0x7) | ((int)raw_bytes[1] << 3) | ((int)raw_bytes[2] << 11);
    if (data->first_partition_size > num_bytes - twp__UNCOMPRESSED_VP8_HEADER_SIZE) return 0;

    int magic = (int)raw_bytes[3] | ((int)raw_bytes[4] << 8) | ((int)raw_bytes[5] << 16);
    if (magic != 0x2a019d) return 0;

    // todo: currently, the upscaling options have no effect, consistent with libwebp's behavior. in the future,
    //       it might be worth adding an option to actually upscale the image, rather than just ignoring this setting
    data->width = (int)raw_bytes[6] | (((int)raw_bytes[7] & 0x3f) << 8);
    data->upscaling_x = (twp__upscaling)(raw_bytes[7] >> 6);
    data->height = (int)raw_bytes[8] | (((int)raw_bytes[9] & 0x3f) << 8);
    data->upscaling_y = (twp__upscaling)(raw_bytes[9] >> 6);

    data->luma_width = twp__div_round_up(data->width, 16) * 16;
    data->luma_height = twp__div_round_up(data->height, 16) * 16;
    data->luma_stride = 1 + data->luma_width + 4;
    data->chroma_width = data->luma_width / 2;
    data->chroma_height = data->luma_height / 2;
    data->chroma_stride = 1 + data->chroma_width;
    data->mbs_per_row = data->luma_width / 16;
    data->mbs_per_col = data->luma_height / 16;
    data->num_mbs = data->mbs_per_row * data->mbs_per_col;
    data->sbs_per_row = data->mbs_per_row * 4;
    data->sbs_per_col = data->mbs_per_col * 4;
    data->num_sbs = data->sbs_per_row * data->sbs_per_col;

    twp__init_arith_decoder(&data->first_partition_dec, (uint8_t *)raw_bytes + twp__UNCOMPRESSED_VP8_HEADER_SIZE, data->first_partition_size);
    twp__arith_dec *dec = &data->first_partition_dec;

    int color_space = twp__read_arith_literal(dec, 1);
    if (color_space != 0) return 0; // 0 is the only valid value, 1 is "reserved for future use" (meaning probably never)

    data->require_clamping = twp__read_arith_literal(dec, 1);

    // segmentation_enabled is confusing. it doesn't actually tell you whether you have to read segment_ids
    // for the macroblocks; that's being done by update_mb_segmentation_map. so, if segmentation_enabled is
    // true, but update_mb_segmentation_map is false, then you *must not* read segment_ids; instead, all
    // segment_ids are assumed to be 0. note that update_segment_feature_data might have still been true,
    // in which case macroblocks *do not* just have the same behavior as if segmentation_enabled was false.
    // however, if segmentation_enabled is true, but both update_segment_feature_data and update_mb_segmentation_map
    // are false, then i think that would be the same as segmentation_enabled being false.

    data->segmentation_enabled = twp__read_arith_literal(dec, 1);
    if (data->segmentation_enabled) {
        int update_mb_segmentation_map = twp__read_arith_literal(dec, 1);
        int update_segment_feature_data = twp__read_arith_literal(dec, 1);

        if (update_segment_feature_data) {
            data->segment_mode = (twp__segment_mode)twp__read_arith_literal(dec, 1);

            for (int i = 0; i < twp__MAX_SEGMENTS; ++i) {
                if (twp__read_arith_literal(dec, 1))
                    data->segments[i].quant_update = twp__read_arith_signed_literal(dec, 7);
            }

            for (int i = 0; i < twp__MAX_SEGMENTS; ++i) {
                if (twp__read_arith_literal(dec, 1))
                    data->segments[i].lf_update = twp__read_arith_signed_literal(dec, 6);
            }
        }

        if (update_mb_segmentation_map) {
            for (int i = 0; i < twp__arrlen(data->segment_id_tree_probs); ++i) {
                if (twp__read_arith_literal(dec, 1))
                    data->segment_id_tree_probs[i] = twp__read_arith_literal(dec, 8);
            }
        } else {
            data->all_mbs_are_segment_0 = 1;
        }
    }

    data->loop_filter_type = (twp__filter_type)twp__read_arith_literal(dec, 1);
    data->loop_filter_level = twp__read_arith_literal(dec, 6);
    data->loop_filter_sharpness = twp__read_arith_literal(dec, 3);

    data->lf_adj_enabled = twp__read_arith_literal(dec, 1);
    if (data->lf_adj_enabled) {
        int mode_ref_lf_delta_update = twp__read_arith_literal(dec, 1);
        if (mode_ref_lf_delta_update == 0) {
            // i think this flag is 0 when you want to re-use the data from the last frame, but that doesn't make sense for an image,
            // so my assumption is it should always be 1 for webp?
            return 0;
        }

        for (int i = 0; i < 4; ++i) {
            int ref_frame_delta_update_flag = twp__read_arith_literal(dec, 1);
            if (ref_frame_delta_update_flag)
                data->lf_adj_ref_frame[i] = twp__read_arith_signed_literal(dec, 6);
        }

        for (int i = 0; i < 4; ++i) {
            int mb_mode_delta_update_flag = twp__read_arith_literal(dec, 1);
            if (mb_mode_delta_update_flag)
                data->lf_adj_mb_mode[i] = twp__read_arith_signed_literal(dec, 6);
        }
    }

    int log2_nbr_of_dct_partitions = twp__read_arith_literal(dec, 2);
    data->num_dct_partitions = 1 << log2_nbr_of_dct_partitions;
    twp__assert(data->num_dct_partitions <= twp__MAX_PARITIONS);

    int y_ac_quant_idx = twp__read_arith_literal(dec, 7);
    int y_dc_quant_idx_delta = twp__read_arith_flagged_signed_literal(dec, 4, 0);
    int y2_dc_quant_idx_delta = twp__read_arith_flagged_signed_literal(dec, 4, 0);
    int y2_ac_quant_idx_delta = twp__read_arith_flagged_signed_literal(dec, 4, 0);
    int uv_dc_quant_idx_delta = twp__read_arith_flagged_signed_literal(dec, 4, 0);
    int uv_ac_quant_idx_delta = twp__read_arith_flagged_signed_literal(dec, 4, 0);

    data->frame_quant_idx_base = y_ac_quant_idx;
    data->quant_idx_deltas[twp__DCT_PLANE_Y][twp__DC] = y_dc_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_Y][twp__AC] = 0;
    data->quant_idx_deltas[twp__DCT_PLANE_Y2][twp__DC] = y2_dc_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_Y2][twp__AC] = y2_ac_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_U][twp__DC] = uv_dc_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_U][twp__AC] = uv_ac_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_V][twp__DC] = uv_dc_quant_idx_delta;
    data->quant_idx_deltas[twp__DCT_PLANE_V][twp__AC] = uv_ac_quant_idx_delta;

    // as far as i can tell this doesn't matter for webp images, so it's just ignored
    /*int refresh_entropy_probs = */twp__read_arith_literal(dec, 1);

    for (int i = 0; i < twp__arrlen(twp__coeff_update_probs); ++i) {
        for (int j = 0; j < twp__arrlen(twp__coeff_update_probs[0]); ++j) {
            for (int k = 0; k < twp__arrlen(twp__coeff_update_probs[0][0]); ++k) {
                for (int l = 0; l < twp__arrlen(twp__coeff_update_probs[0][0][0]); ++l) {
                    int coeff_prob_update_flag = twp__read_arith_bit(dec, twp__coeff_update_probs[i][j][k][l]);
                    if (coeff_prob_update_flag)
                        data->coeff_probs[i][j][k][l] = twp__read_arith_literal(dec, 8);;
                }
            }
        }
    }

    // this specifies if skipping macroblocks with only 0's in them is enabled
    // the spec calls this mb_no_skip_coeff, whis makes absolutely no sense at all?
    int enable_mb_skipping = twp__read_arith_literal(dec, 1);

    // if skipping is enabled, the probability for reading twp__mb_mode.skip (mb_skip_coeff in the spec)
    int skip_mb_flag_prob = 0;
    if (enable_mb_skipping)
        skip_mb_flag_prob = twp__read_arith_literal(dec, 8);

    // frame header finished, continue with macroblock prediction records

    data->mb_infos = (twp__mb_info *)calloc(data->num_mbs, sizeof(*data->mb_infos));

    for (int mb_idx = 0; mb_idx < data->num_mbs; ++mb_idx) {
        twp__mb_info *mb_info = data->mb_infos + mb_idx;

        int mb_x = mb_idx % data->mbs_per_row;
        int mb_y = mb_idx / data->mbs_per_row;
        mb_info->x = mb_x;
        mb_info->y = mb_y;

        mb_info->segment_id = 0;
        if (data->segmentation_enabled && !data->all_mbs_are_segment_0)
            mb_info->segment_id = (uint8_t)twp__read_arith_tree(dec, twp__segment_id_tree, data->segment_id_tree_probs, 0);

        if (enable_mb_skipping)
            mb_info->skip = (uint8_t)twp__read_arith_bit(dec, skip_mb_flag_prob);

        mb_info->y_mode = (uint8_t)twp__read_arith_tree(dec, twp__mb_y_mode_tree, twp__mb_y_mode_tree_probs, 0);
        if (mb_info->y_mode == twp__MB_MODE_B_PRED) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    int abs_x = mb_x*4 + x;
                    int abs_y = mb_y*4 + y;

                    int left;
                    if (abs_x > 0) {
                        if (x == 0)
                            left = mb_info[-1].y_sb_modes[y*4 + 3];
                        else
                            left = mb_info->y_sb_modes[y*4 + (x-1)];
                    } else {
                        left = twp__SB_MODE_DC_PRED;
                    }

                    int above;
                    if (abs_y > 0) {
                        if (y == 0)
                            above = mb_info[-data->mbs_per_row].y_sb_modes[3*4 + x];
                        else
                            above = mb_info->y_sb_modes[(y-1)*4 + x];
                    } else {
                        above = twp__SB_MODE_DC_PRED;
                    }

                    int sb_mode = twp__read_arith_tree(dec, twp__sb_mode_tree, twp__sb_mode_tree_probs[above][left], 0);
                    mb_info->y_sb_modes[y*4 + x] = (uint8_t)sb_mode;
                }
            }
        } else {
            // we also need to set the submodes to make the above/left thing for MODE_B work
            uint8_t sb_mode = 0;
            switch (mb_info->y_mode) {
                case twp__MB_MODE_DC_PRED: sb_mode = twp__SB_MODE_DC_PRED; break;
                case twp__MB_MODE_H_PRED: sb_mode = twp__SB_MODE_HE_PRED; break;
                case twp__MB_MODE_V_PRED: sb_mode = twp__SB_MODE_VE_PRED; break;
                case twp__MB_MODE_TM_PRED: sb_mode = twp__SB_MODE_TM_PRED; break;
                default: sb_mode = 0; twp__assert(0); break;
            }
            for (int i = 0; i < 16; ++i)
                mb_info->y_sb_modes[i] = sb_mode;
        }

        mb_info->uv_mode = (uint8_t)twp__read_arith_tree(dec, twp__mb_uv_mode_tree, twp__mb_uv_mode_tree_probs, 0);
    }

    int num_bytes_for_partition_sizes = (data->num_dct_partitions-1) * 3;
    if (twp__UNCOMPRESSED_VP8_HEADER_SIZE + data->first_partition_size + num_bytes_for_partition_sizes > num_bytes)
        return 0;

    int dct_partitions_start_offset = twp__UNCOMPRESSED_VP8_HEADER_SIZE + data->first_partition_size + num_bytes_for_partition_sizes;

    int partition_size_sum = 0;
    for (int i = 0; i < data->num_dct_partitions-1; ++i) {
        twp__dct_partition *p = data->dct_partitions + i;
        int partition_size =
            (int)raw_bytes[twp__UNCOMPRESSED_VP8_HEADER_SIZE + data->first_partition_size + i*3 + 0] |
            ((int)raw_bytes[twp__UNCOMPRESSED_VP8_HEADER_SIZE + data->first_partition_size + i*3 + 1] << 8) |
            ((int)raw_bytes[twp__UNCOMPRESSED_VP8_HEADER_SIZE + data->first_partition_size + i*3 + 2] << 16);
        if (partition_size < 0) return 0;
        if (partition_size >= num_bytes) return 0;
        p->offset = partition_size_sum;
        p->size = partition_size;
        partition_size_sum += partition_size;
    }
    twp__dct_partition *last_p = &data->dct_partitions[data->num_dct_partitions-1];
    last_p->offset = partition_size_sum;
    last_p->size = num_bytes - dct_partitions_start_offset - partition_size_sum;
    if (last_p->size < 1) return 0;
    if (dct_partitions_start_offset + last_p->offset + last_p->size > num_bytes) return 0;
    for (int i = 0; i < data->num_dct_partitions; ++i) {
        twp__dct_partition *p = data->dct_partitions + i;
        twp__init_arith_decoder(&p->dec, raw_bytes + dct_partitions_start_offset + p->offset, p->size);
    }

    return !dec->err;
}

static void twp__build_quant_table(twp__vp8_data *data, twp__mb_info *mb_info,
            int quant_table[twp__NUM_DCT_PLANES][twp__NUM_DCT_COEFF_TYPES])
{
    for (int plane = 0; plane < twp__NUM_DCT_PLANES; ++plane) {
        for (int coeff_type = 0; coeff_type < twp__NUM_DCT_COEFF_TYPES; ++coeff_type) {
            int quant_idx_base = data->frame_quant_idx_base;
            if (data->segmentation_enabled) {
                if (data->segment_mode == twp__SEGMENT_MODE_DELTA)
                    quant_idx_base += data->segments[mb_info->segment_id].quant_update;
                else if (data->segment_mode == twp__SEGMENT_MODE_ABSOLUTE)
                    quant_idx_base = data->segments[mb_info->segment_id].quant_update;
                else
                    twp__assert(0);
            }

            int quant_idx = quant_idx_base + data->quant_idx_deltas[plane][coeff_type];
            int quant = twp__get_quant((twp__dct_coeff_type)coeff_type, quant_idx);

            if (plane == twp__DCT_PLANE_Y2) {
                if (coeff_type == twp__DC) {
                    quant *= 2;
                } else {
                    quant = quant * 155 / 100;
                    if (quant < 8) quant = 8;
                }
            } else if (plane == twp__DCT_PLANE_U || plane == twp__DCT_PLANE_V) {
                if (coeff_type == twp__DC) {
                    if (quant > 132) quant = 132;
                }
            }

            quant_table[plane][coeff_type] = quant;
        }
    }
}

twp__INLINE static int twp__calc_mb_offset(int x, int y, int stride, int chroma)
{
    int block_size = chroma ? 8 : 16;
    int mb_offset = ((y*block_size + 1) * stride) + (x*block_size + 1);
    return mb_offset;
}

static int twp__read_yuv_data(twp__vp8_data *data, twp_format format)
{
    int y_bufsize = data->luma_stride * (data->luma_height+1);
    int uv_bufsize = data->chroma_stride * (data->chroma_height+1);
    int a_bufsize = (format == twp_FORMAT_YUVA) ? (data->width * data->height) : 0;
    data->plane_y = (uint8_t *)malloc(y_bufsize + uv_bufsize*2 + a_bufsize);
    data->plane_u = data->plane_y + y_bufsize;
    data->plane_v = data->plane_u + uv_bufsize;

    // the spec does not say what the the value of the top left out of bounds value should be.. but it's 127
    // https://codec-devel.webmproject.narkive.com/RBobMxzF/out-of-bound-value-of-p
    for (int i = 0; i < data->luma_stride; ++i) {
        data->plane_y[i] = 127;
    }
    for (int i = 1; i < data->luma_height+1; ++i) {
        data->plane_y[i*data->luma_stride] = 129;
    }
    for (int i = 0; i < data->chroma_stride; ++i) {
        data->plane_u[i] = 127;
        data->plane_v[i] = 127;
    }
    for (int i = 1; i < data->chroma_height+1; ++i) {
        data->plane_u[i*data->chroma_stride] = 129;
        data->plane_v[i*data->chroma_stride] = 129;
    }

    uint8_t nz_left[twp__NUM_DCT_PLANES][4];
    uint8_t *nz_above[twp__NUM_DCT_PLANES];
    int nz_above_alloc_size = data->mbs_per_row*4 + data->mbs_per_row + data->mbs_per_row*2 + data->mbs_per_row*2; // y + y2 + u + v
    uint8_t *nz_above_alloc = (uint8_t *)calloc(nz_above_alloc_size, sizeof(**nz_above));
    nz_above[twp__DCT_PLANE_Y] = nz_above_alloc;
    nz_above[twp__DCT_PLANE_Y2] = nz_above[twp__DCT_PLANE_Y] + data->mbs_per_row*4;
    nz_above[twp__DCT_PLANE_U] = nz_above[twp__DCT_PLANE_Y2] + data->mbs_per_row;
    nz_above[twp__DCT_PLANE_V] = nz_above[twp__DCT_PLANE_U] + data->mbs_per_row*2;

    int curr_partition_idx = 0;
    for (int mb_y = 0; mb_y < data->mbs_per_col; ++mb_y) {
        twp__arith_dec *dec = &data->dct_partitions[curr_partition_idx].dec;
        memset(nz_left, 0, sizeof(nz_left));

        // make sure the the special subblock prediction case handling below works correctly when mb_x == mbs_per_row-1
        memset(data->plane_y + ((mb_y*16) * data->luma_stride) + data->luma_width + 1,
               data->plane_y[((mb_y*16) * data->luma_stride) + data->luma_width],
               4);

        for (int mb_x = 0; mb_x < data->mbs_per_row; ++mb_x) {
            twp__mb_info *mb_info = &data->mb_infos[mb_y*data->mbs_per_row + mb_x];
            int luma_mb_offset = twp__calc_mb_offset(mb_x, mb_y, data->luma_stride, 0);
            int chroma_mb_offset = twp__calc_mb_offset(mb_x, mb_y, data->chroma_stride, 1);
            int have_y2 = (mb_info->y_mode != twp__MB_MODE_B_PRED);

            twp__predict_macroblock(data->plane_u, mb_info, 1, chroma_mb_offset, data->chroma_stride);
            twp__predict_macroblock(data->plane_v, mb_info, 1, chroma_mb_offset, data->chroma_stride);
            if (have_y2) {
                // if the mode is B_PRED, we need to fully reconstruct each subblock before going to the next subblock,
                // which means we can't just predict the entire 16x16 block first
                twp__predict_macroblock(data->plane_y, mb_info, 0, luma_mb_offset, data->luma_stride);
            } else {
                // handle special subblock prediction case when sb_x == 3
                uint8_t *pred_row = data->plane_y + ((mb_y*16) * data->luma_stride);
                uint8_t *row_at = pred_row;
                for (int y = 0; y < 4; ++y) {
                    row_at += 4*data->luma_stride;
                    memcpy(row_at + mb_x*16 + 17, pred_row + mb_x*16 + 17, 4);
                }
            }

            if (mb_info->skip) {
                if (have_y2) {
                    nz_left[twp__DCT_PLANE_Y2][0] = 0;
                    nz_above[twp__DCT_PLANE_Y2][mb_x] = 0;
                    mb_info->skip_sb_filtering = 1;
                }

                for (int sb_y = 0; sb_y < 4; ++sb_y) {
                    for (int sb_x = 0; sb_x < 4; ++sb_x) {
                        nz_left[twp__DCT_PLANE_Y][sb_y] = 0;
                        nz_above[twp__DCT_PLANE_Y][mb_x*4 + sb_x] = 0;

                        if (!have_y2)
                            twp__predict_luma_subblock(data->plane_y, mb_info, sb_x, sb_y, luma_mb_offset, data->luma_stride);
                    }
                }

                for (int sb_y = 0; sb_y < 2; ++sb_y) {
                    for (int sb_x = 0; sb_x < 2; ++sb_x) {
                        nz_left[twp__DCT_PLANE_U][sb_y] = 0;
                        nz_left[twp__DCT_PLANE_V][sb_y] = 0;
                        nz_above[twp__DCT_PLANE_U][mb_x*2 + sb_x] = 0;
                        nz_above[twp__DCT_PLANE_V][mb_x*2 + sb_x] = 0;
                    }
                }

                continue;
            }

            // we could build the quant table outside of the loop if this ever becomes a hotspot..
            int quant_table[twp__NUM_DCT_PLANES][twp__NUM_DCT_COEFF_TYPES];
            twp__build_quant_table(data, mb_info, quant_table);

            int all_zero = 1;
            short dc_coeffs[16];
            short residue[16];

            if (have_y2) {
                all_zero &= twp__read_residual_block(dec, twp__DCT_TYPE_Y2, dc_coeffs, data->coeff_probs,
                        &nz_left[twp__DCT_PLANE_Y2][0], &nz_above[twp__DCT_PLANE_Y2][mb_x],
                        quant_table[twp__DCT_PLANE_Y2][twp__DC], quant_table[twp__DCT_PLANE_Y2][twp__AC]);
                twp__iwht(dc_coeffs);
            }

            for (int sb_y = 0; sb_y < 4; ++sb_y) {
                for (int sb_x = 0; sb_x < 4; ++sb_x) {
                    all_zero &= twp__read_residual_block(dec, have_y2 ? twp__DCT_TYPE_Y_WITHOUT_DC : twp__DCT_TYPE_Y_WITH_DC,
                            residue, data->coeff_probs, &nz_left[twp__DCT_PLANE_Y][sb_y], &nz_above[twp__DCT_PLANE_Y][sb_x + mb_x*4],
                            quant_table[twp__DCT_PLANE_Y][twp__DC], quant_table[twp__DCT_PLANE_Y][twp__AC]);
                    if (!have_y2)
                        twp__predict_luma_subblock(data->plane_y, mb_info, sb_x, sb_y, luma_mb_offset, data->luma_stride);
                    else
                        residue[0] = dc_coeffs[sb_y*4 + sb_x];

                    twp__idct(residue, data->plane_y, luma_mb_offset, sb_x, sb_y, data->luma_stride);
                }
            }

            for (int sb_y = 0; sb_y < 2; ++sb_y) {
                for (int sb_x = 0; sb_x < 2; ++sb_x) {
                    all_zero &= twp__read_residual_block(dec, twp__DCT_TYPE_UV, residue, data->coeff_probs,
                            &nz_left[twp__DCT_PLANE_U][sb_y], &nz_above[twp__DCT_PLANE_U][sb_x + mb_x*2],
                            quant_table[twp__DCT_PLANE_U][twp__DC], quant_table[twp__DCT_PLANE_U][twp__AC]);
                    twp__idct(residue, data->plane_u, chroma_mb_offset, sb_x, sb_y, data->chroma_stride);
                }
            }

            for (int sb_y = 0; sb_y < 2; ++sb_y) {
                for (int sb_x = 0; sb_x < 2; ++sb_x) {
                    all_zero &= twp__read_residual_block(dec, twp__DCT_TYPE_UV, residue, data->coeff_probs,
                            &nz_left[twp__DCT_PLANE_V][sb_y], &nz_above[twp__DCT_PLANE_V][sb_x + mb_x*2],
                            quant_table[twp__DCT_PLANE_V][twp__DC], quant_table[twp__DCT_PLANE_V][twp__AC]);
                    twp__idct(residue, data->plane_v, chroma_mb_offset, sb_x, sb_y, data->chroma_stride);
                }
            }

            mb_info->skip_sb_filtering = (all_zero && have_y2);
        }

        ++curr_partition_idx;
        if (curr_partition_idx >= data->num_dct_partitions)
            curr_partition_idx = 0;
    }

    free(nz_above_alloc);

    for (int i = 0; i < data->num_dct_partitions; ++i) {
        if (data->dct_partitions[i].dec.err)
            return 0;
    }

    return 1;
}

static int twp__get_filter_level(twp__vp8_data *data, twp__mb_info *mb_info)
{
    // how this works is not explained in the spec at all. you just have to decipher their shitty c code

    int filter_level = data->loop_filter_level;
    if (filter_level == 0) return 0; // if 0 at the frame level, filtering should be skipped

    if (data->segmentation_enabled) {
        if (data->segment_mode == twp__SEGMENT_MODE_ABSOLUTE)
            filter_level = data->segments[mb_info->segment_id].lf_update;
        else if (data->segment_mode == twp__SEGMENT_MODE_DELTA)
            filter_level += data->segments[mb_info->segment_id].lf_update;
        else
            twp__assert(0);
    }
    filter_level = twp__clamp(filter_level, 0, 63);

    if (data->lf_adj_enabled) {
        filter_level += data->lf_adj_ref_frame[0];
        if (mb_info->y_mode == twp__MB_MODE_B_PRED)
            filter_level += data->lf_adj_mb_mode[0];
    }
    filter_level = twp__clamp(filter_level, 0, 63);

    return filter_level;
}

static int twp__get_interior_limit(int filter_level, int sharpness)
{
    // the maximum value of this is the same as filter_level, meaning 63

    int interior_limit = filter_level;
    if (sharpness != 0) {
        if (sharpness > 4)
            interior_limit >>= 2;
        else
            interior_limit >>= 1;

        int max_interior_limit = 9 - sharpness;
        if (interior_limit > max_interior_limit)
            interior_limit = max_interior_limit;
    }
    if (interior_limit < 0)
        interior_limit = 0;

    return interior_limit;
}

static int twp__get_hev_threshold(int filter_level)
{
    int hev_threshold = 0;
    if (filter_level >= 15) ++hev_threshold;
    if (filter_level >= 40) ++hev_threshold;
    return hev_threshold;
}

static void twp__get_edge_limits(int filter_level, int interior_limit, int *mb, int *sb)
{
    // maximum values:
    //   193 for mb
    //   189 for sb
    *mb = ((filter_level + 2) * 2) + interior_limit;
    *sb = (filter_level * 2) + interior_limit;
}

#ifndef twp__SSE2

twp__INLINE static int twp__lf_clamp(int val)
{
    return twp__clamp(val, -128, 127);
}

twp__INLINE static int twp__lf_u2s(int val)
{
    return val - 128;
}

twp__INLINE static uint8_t twp__lf_s2u(int val)
{
    return (uint8_t)(twp__lf_clamp(val) + 128);
}

static int twp__simple_threshold(int edge_limit, int p1, int p0, int q0, int q1)
{
    return (twp__abs(p0 - q0)*2 + twp__abs(p1 - q1)/2) <= edge_limit;
}

static int twp__is_hev(int hev_threshold, int p1, int p0, int q0, int q1)
{
    return (twp__abs(p1 - p0) > hev_threshold) || (twp__abs(q1 - q0) > hev_threshold);
}

static int twp__normal_threshold(int edge_limit, int interior_limit,
        int p3, int p2, int p1, int p0, int q0, int q1, int q2, int q3)
{
    return (twp__simple_threshold(edge_limit, p1, p0, q0, q1)) &&
           (twp__abs(p3 - p2) <= interior_limit) &&
           (twp__abs(p2 - p1) <= interior_limit) &&
           (twp__abs(p1 - p0) <= interior_limit) &&
           (twp__abs(q0 - q1) <= interior_limit) &&
           (twp__abs(q1 - q2) <= interior_limit) &&
           (twp__abs(q2 - q3) <= interior_limit);
}

static int twp__filter_common(int use_outer_taps, int p1, int *p0, int *q0, int q1)
{
    int a;
    if (use_outer_taps)
        a = twp__lf_clamp(p1 - q1) + 3*(*q0 - *p0);
    else
        a = 3*(*q0 - *p0);
    a = twp__lf_clamp(a);

    int b = twp__sra(twp__lf_clamp(a + 3), 3);

    int c = twp__sra(twp__lf_clamp(a + 4), 3);

    *q0 = twp__lf_clamp(*q0 - c);
    *p0 = twp__lf_clamp(*p0 + b);

    return c;
}

static void twp__normal_filter_get_pixels(uint8_t *plane, int stride, int offset, int vert, int i,
                            uint8_t **p3_ptr, uint8_t **p2_ptr, uint8_t **p1_ptr, uint8_t **p0_ptr,
                            uint8_t **q0_ptr, uint8_t **q1_ptr, uint8_t **q2_ptr, uint8_t **q3_ptr,
                            int *p3, int *p2, int *p1, int *p0, int *q0, int *q1, int *q2, int *q3)
{
    if (vert)
        *p3_ptr = &plane[offset - 4*stride + i];
    else
        *p3_ptr = &plane[offset + i*stride - 4];
    *p2_ptr = *p3_ptr + (vert ? 1*stride : 1);
    *p1_ptr = *p3_ptr + (vert ? 2*stride : 2);
    *p0_ptr = *p3_ptr + (vert ? 3*stride : 3);
    *q0_ptr = *p3_ptr + (vert ? 4*stride : 4);
    *q1_ptr = *p3_ptr + (vert ? 5*stride : 5);
    *q2_ptr = *p3_ptr + (vert ? 6*stride : 6);
    *q3_ptr = *p3_ptr + (vert ? 7*stride : 7);

    *p3 = twp__lf_u2s(**p3_ptr);
    *p2 = twp__lf_u2s(**p2_ptr);
    *p1 = twp__lf_u2s(**p1_ptr);
    *p0 = twp__lf_u2s(**p0_ptr);
    *q0 = twp__lf_u2s(**q0_ptr);
    *q1 = twp__lf_u2s(**q1_ptr);
    *q2 = twp__lf_u2s(**q2_ptr);
    *q3 = twp__lf_u2s(**q3_ptr);
}

static void twp__normal_filter_sb_(int edge_limit, int interior_limit, int hev_threshold,
                            uint8_t *plane, int stride, int mb_offset, int vert, int chroma)
{
    for (int j = 0; j < (chroma ? 1 : 3); ++j) {
        int sb_offset = mb_offset;
        if (vert)
            sb_offset += (j+1)*4*stride;
        else
            sb_offset += (j+1)*4;

        for (int i = 0; i < (chroma ? 8 : 16); ++i) {
            uint8_t *p3_ptr, *p2_ptr, *p1_ptr, *p0_ptr, *q0_ptr, *q1_ptr, *q2_ptr, *q3_ptr;
            int p3, p2, p1, p0, q0, q1, q2, q3;
            twp__normal_filter_get_pixels(plane, stride, sb_offset, vert, i,
                                          &p3_ptr, &p2_ptr, &p1_ptr, &p0_ptr,
                                          &q0_ptr, &q1_ptr, &q2_ptr, &q3_ptr,
                                          &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);

            if (!twp__normal_threshold(edge_limit, interior_limit, p3, p2, p1, p0, q0, q1, q2, q3))
                continue;

            int hev = twp__is_hev(hev_threshold, p1, p0, q0, q1);
            int a = twp__filter_common(hev, p1, &p0, &q0, q1);
            *p0_ptr = twp__lf_s2u(p0);
            *q0_ptr = twp__lf_s2u(q0);
            a = twp__sra((a + 1), 1);
            if (!hev) {
                *q1_ptr = twp__lf_s2u(q1 - a);
                *p1_ptr = twp__lf_s2u(p1 + a);
            }
        }
    }
}

static void twp__normal_filter_sb(int edge_limit, int interior_limit, int hev_threshold,
                    uint8_t *plane0, uint8_t *plane1, int stride, int mb_offset, int vert)
{
    // this function makes it so the simd version has the same function signature, which means we don't
    // need a bunch of ifdefs
    if (plane0 && plane1) {
        twp__normal_filter_sb_(edge_limit, interior_limit, hev_threshold, plane0, stride, mb_offset, vert, 1);
        twp__normal_filter_sb_(edge_limit, interior_limit, hev_threshold, plane1, stride, mb_offset, vert, 1);
    } else {
        twp__normal_filter_sb_(edge_limit, interior_limit, hev_threshold, plane0, stride, mb_offset, vert, 0);
    }
}

static void twp__normal_filter_mb_(int edge_limit, int interior_limit, int hev_threshold,
                        uint8_t *plane, int stride, int mb_offset, int vert, int chroma)
{
    for (int i = 0; i < (chroma ? 8 : 16); ++i) {
        uint8_t *p3_ptr, *p2_ptr, *p1_ptr, *p0_ptr, *q0_ptr, *q1_ptr, *q2_ptr, *q3_ptr;
        int p3, p2, p1, p0, q0, q1, q2, q3;
        twp__normal_filter_get_pixels(plane, stride, mb_offset, vert, i,
                                      &p3_ptr, &p2_ptr, &p1_ptr, &p0_ptr,
                                      &q0_ptr, &q1_ptr, &q2_ptr, &q3_ptr,
                                      &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);

        if (!twp__normal_threshold(edge_limit, interior_limit, p3, p2, p1, p0, q0, q1, q2, q3))
            continue;

        if (twp__is_hev(hev_threshold, p1, p0, q0, q1)) {
            twp__filter_common(1, p1, &p0, &q0, q1);
            *p0_ptr = twp__lf_s2u(p0);
            *q0_ptr = twp__lf_s2u(q0);
        } else {
            int w = twp__lf_clamp(twp__lf_clamp(p1 - q1) + 3*(q0 - p0));

            int a = twp__lf_clamp(twp__sra(27*w + 63, 7));
            *q0_ptr = twp__lf_s2u(q0 - a);
            *p0_ptr = twp__lf_s2u(p0 + a);

            int b = twp__lf_clamp(twp__sra(18*w + 63, 7));
            *q1_ptr = twp__lf_s2u(q1 - b);
            *p1_ptr = twp__lf_s2u(p1 + b);

            int c = twp__lf_clamp(twp__sra(9*w + 63, 7));
            *q2_ptr = twp__lf_s2u(q2 - c);
            *p2_ptr = twp__lf_s2u(p2 + c);
        }
    }
}

static void twp__normal_filter_mb(int edge_limit, int interior_limit, int hev_threshold,
                    uint8_t *plane0, uint8_t *plane1, int stride, int mb_offset, int vert)
{
    // this function makes it so the simd version has the same function signature, which means we don't
    // need a bunch of ifdefs
    if (plane0 && plane1) {
        twp__normal_filter_mb_(edge_limit, interior_limit, hev_threshold, plane0, stride, mb_offset, vert, 1);
        twp__normal_filter_mb_(edge_limit, interior_limit, hev_threshold, plane1, stride, mb_offset, vert, 1);
    } else {
        twp__normal_filter_mb_(edge_limit, interior_limit, hev_threshold, plane0, stride, mb_offset, vert, 0);
    }
}

static void twp__simple_filter_mb(int edge_limit, uint8_t *plane, int stride, int mb_offset, int vert)
{
    for (int i = 0; i < 16; ++i) {
        uint8_t *p1_ptr, *p0_ptr, *q0_ptr, *q1_ptr;
        if (vert)
            p1_ptr = &plane[mb_offset - 2*stride + i];
        else
            p1_ptr = &plane[mb_offset + i*stride - 2];
        p0_ptr = p1_ptr + (vert ? 1*stride : 1);
        q0_ptr = p1_ptr + (vert ? 2*stride : 2);
        q1_ptr = p1_ptr + (vert ? 3*stride : 3);

        int p1 = twp__lf_u2s(*p1_ptr);
        int p0 = twp__lf_u2s(*p0_ptr);
        int q0 = twp__lf_u2s(*q0_ptr);
        int q1 = twp__lf_u2s(*q1_ptr);
        if (twp__simple_threshold(edge_limit, p1, p0, q0, q1)) {
            twp__filter_common(1, p1, &p0, &q0, q1);
            *p0_ptr = twp__lf_s2u(p0);
            *q0_ptr = twp__lf_s2u(q0);
        }
    }
}

#else

twp__INLINE static __m128i twp__arith_right_shift_bytes_by_3(__m128i val)
{
    __m128i t0 = _mm_unpacklo_epi8(_mm_setzero_si128(), val);
    __m128i t1 = _mm_unpackhi_epi8(_mm_setzero_si128(), val);
    __m128i t2 = _mm_srai_epi16(t0, 11);
    __m128i t3 = _mm_srai_epi16(t1, 11);
    __m128i res = _mm_packs_epi16(t2, t3);
    return res;
}

twp__INLINE static __m128i twp__logical_right_shift_bytes_by_1(__m128i val)
{
    __m128i lsb_mask = _mm_set1_epi8(-2); // 0b11111110
    __m128i tmp = _mm_and_si128(val, lsb_mask);
    __m128i res = _mm_srli_epi16(tmp, 1);
    return res;
}

twp__INLINE static void twp__transpose_bytes_16x8(__m128i *i0, __m128i *i1, __m128i *i2, __m128i *i3,
                                                  __m128i *i4, __m128i *i5, __m128i *i6, __m128i *i7)
{
    // i0 =                                          0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    // i1 =                                         16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
    // i2 =                                         32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  47
    // i3 =                                         48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63
    // i4 =                                         64  65  66  67  68  69  70  71  72  73  74  75  76  77  78  79
    // i5 =                                         80  81  82  83  84  85  86  87  88  89  90  91  92  93  94  95
    // i6 =                                         96  97  98  99 100 101 102 103 104 105 106 107 108 109 110 111
    // i7 =                                        112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127

    __m128i t0 = _mm_unpacklo_epi8(*i0, *i1);   //   0  16   1  17   2  18   3  19   4  20   5  21   6  22   7  23
    __m128i t1 = _mm_unpacklo_epi8(*i2, *i3);   //  32  48  33  49  34  50  35  51  36  52  37  53  38  54  39  55
    __m128i t2 = _mm_unpacklo_epi8(*i4, *i5);   //  64  80  65  81  66  82  67  83  68  84  69  85  70  86  71  87
    __m128i t3 = _mm_unpacklo_epi8(*i6, *i7);   //  96 112  97 113  98 114  99 115 100 116 101 117 102 118 103 119
    __m128i t4 = _mm_unpackhi_epi8(*i0, *i1);   //   8  24   9  25  10  26  11  27  12  28  13  29  14  30  15  31
    __m128i t5 = _mm_unpackhi_epi8(*i2, *i3);   //  40  56  41  57  42  58  43  59  44  60  45  61  46  62  47  63
    __m128i t6 = _mm_unpackhi_epi8(*i4, *i5);   //  72  88  73  89  74  90  75  91  76  92  77  93  78  94  79  95
    __m128i t7 = _mm_unpackhi_epi8(*i6, *i7);   // 104 120 105 121 106 122 107 123 108 124 109 125 110 126 111 127

    __m128i t8 = _mm_unpacklo_epi8(t0, t4);     //   0   8  16  24   1   9  17  25   2  10  18  26   3  11  19  27
    __m128i t9 = _mm_unpacklo_epi8(t1, t5);     //  32  40  48  56  33  41  49  57  34  42  50  58  35  43  51  59
    __m128i t10 = _mm_unpacklo_epi8(t2, t6);    //  64  72  80  88  65  73  81  89  66  74  82  90  67  75  83  91
    __m128i t11 = _mm_unpacklo_epi8(t3, t7);    //  96 104 112 120  97 105 113 121  98 106 114 122  99 107 115 123
    __m128i t12 = _mm_unpackhi_epi8(t0, t4);    //   4  12  20  28   5  13  21  29   6  14  22  30   7  15  23  31
    __m128i t13 = _mm_unpackhi_epi8(t1, t5);    //  36  44  52  60  37  45  53  61  38  46  54  62  39  47  55  63
    __m128i t14 = _mm_unpackhi_epi8(t2, t6);    //  68  76  84  92  69  77  85  93  70  78  86  94  71  79  87  95
    __m128i t15 = _mm_unpackhi_epi8(t3, t7);    // 100 108 116 124 101 109 117 125 102 110 118 126 103 111 119 127

    __m128i t16 = _mm_unpacklo_epi32(t8, t9);   //   0   8  16  24  32  40  48  56   1   9  17  25  33  41  49  57
    __m128i t17 = _mm_unpacklo_epi32(t10, t11); //  64  72  80  88  96 104 112 120  65  73  81  89  97 105 113 121
    __m128i t18 = _mm_unpacklo_epi32(t12, t13); //   4  12  20  28  36  44  52  60   5  13  21  29  37  45  53  61
    __m128i t19 = _mm_unpacklo_epi32(t14, t15); //  68  76  84  92 100 108 116 124  69  77  85  93 101 109 117 125
    __m128i t20 = _mm_unpackhi_epi32(t8, t9);   //   2  10  18  26  34  42  50  58   3  11  19  27  35  43  51  59
    __m128i t21 = _mm_unpackhi_epi32(t10, t11); //  66  74  82  90  98 106 114 122  67  75  83  91  99 107 115 123
    __m128i t22 = _mm_unpackhi_epi32(t12, t13); //   6  14  22  30  38  46  54  62   7  15  23  31  39  47  55  63
    __m128i t23 = _mm_unpackhi_epi32(t14, t15); //  70  78  86  94 102 110 118 126  71  79  87  95 103 111 119 127

    *i0 = _mm_unpacklo_epi64(t16, t17);         //   0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
    *i1 = _mm_unpackhi_epi64(t16, t17);         //   1   9  17  25  33  41  49  57  65  73  81  89  97 105 113 121
    *i2 = _mm_unpacklo_epi64(t20, t21);         //   2  10  18  26  34  42  50  58  66  74  82  90  98 106 114 122
    *i3 = _mm_unpackhi_epi64(t20, t21);         //   3  11  19  27  35  43  51  59  67  75  83  91  99 107 115 123
    *i4 = _mm_unpacklo_epi64(t18, t19);         //   4  12  20  28  36  44  52  60  68  76  84  92 100 108 116 124
    *i5 = _mm_unpackhi_epi64(t18, t19);         //   5  13  21  29  37  45  53  61  69  77  85  93 101 109 117 125
    *i6 = _mm_unpacklo_epi64(t22, t23);         //   6  14  22  30  38  46  54  62  70  78  86  94 102 110 118 126
    *i7 = _mm_unpackhi_epi64(t22, t23);         //   7  15  23  31  39  47  55  63  71  79  87  95 103 111 119 127
}

twp__INLINE static void twp__transpose_bytes_16x8_reverse(__m128i *i0, __m128i *i1, __m128i *i2, __m128i *i3,
                                                          __m128i *i4, __m128i *i5, __m128i *i6, __m128i *i7)
{
    // i0 =                                        0   8  16  24  32  40  48  56  64  72  80  88  96 104 112 120
    // i1 =                                        1   9  17  25  33  41  49  57  65  73  81  89  97 105 113 121
    // i2 =                                        2  10  18  26  34  42  50  58  66  74  82  90  98 106 114 122
    // i3 =                                        3  11  19  27  35  43  51  59  67  75  83  91  99 107 115 123
    // i4 =                                        4  12  20  28  36  44  52  60  68  76  84  92 100 108 116 124
    // i5 =                                        5  13  21  29  37  45  53  61  69  77  85  93 101 109 117 125
    // i6 =                                        6  14  22  30  38  46  54  62  70  78  86  94 102 110 118 126
    // i7 =                                        7  15  23  31  39  47  55  63  71  79  87  95 103 111 119 127

    __m128i t0 = _mm_unpacklo_epi8(*i0, *i1); //   0   1   8   9  16  17  24  25  32  33  40  41  48  49  56  57
    __m128i t1 = _mm_unpacklo_epi8(*i2, *i3); //   2   3  10  11  18  19  26  27  34  35  42  43  50  51  58  59
    __m128i t2 = _mm_unpacklo_epi8(*i4, *i5); //   4   5  12  13  20  21  28  29  36  37  44  45  52  53  60  61
    __m128i t3 = _mm_unpacklo_epi8(*i6, *i7); //   6   7  14  15  22  23  30  31  38  39  46  47  54  55  62  63
    __m128i t4 = _mm_unpackhi_epi8(*i0, *i1); //  64  65  72  73  80  81  88  89  96  97 104 105 112 113 120 121
    __m128i t5 = _mm_unpackhi_epi8(*i2, *i3); //  66  67  74  75  82  83  90  91  98  99 106 107 114 115 122 123
    __m128i t6 = _mm_unpackhi_epi8(*i4, *i5); //  68  69  76  77  84  85  92  93 100 101 108 109 116 117 124 125
    __m128i t7 = _mm_unpackhi_epi8(*i6, *i7); //  70  71  78  79  86  87  94  95 102 103 110 111 118 119 126 127

    __m128i t8 = _mm_unpacklo_epi16(t0, t1);  //   0   1   2   3   8   9  10  11  16  17  18  19  24  25  26  27
    __m128i t9 = _mm_unpacklo_epi16(t4, t5);  //  64  65  66  67  72  73  74  75  80  81  82  83  88  89  90  91
    __m128i t10 = _mm_unpacklo_epi16(t2, t3); //   4   5   6   7  12  13  14  15  20  21  22  23  28  29  30  31
    __m128i t11 = _mm_unpacklo_epi16(t6, t7); //  68  69  70  71  76  77  78  79  84  85  86  87  92  93  94  95
    __m128i t12 = _mm_unpackhi_epi16(t0, t1); //  32  33  34  35  40  41  42  43  48  49  50  51  56  57  58  59
    __m128i t13 = _mm_unpackhi_epi16(t4, t5); //  96  97  98  99 104 105 106 107 112 113 114 115 120 121 122 123
    __m128i t14 = _mm_unpackhi_epi16(t2, t3); //  36  37  38  39  44  45  46  47  52  53  54  55  60  61  62  63
    __m128i t15 = _mm_unpackhi_epi16(t6, t7); // 100 101 102 103 108 109 110 111 116 117 118 119 124 125 126 127

    *i0 = _mm_unpacklo_epi32(t8, t10);        //   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
    *i1 = _mm_unpackhi_epi32(t8, t10);        //  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31
    *i2 = _mm_unpacklo_epi32(t12, t14);       //  32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  47
    *i3 = _mm_unpackhi_epi32(t12, t14);       //  48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63
    *i4 = _mm_unpacklo_epi32(t9, t11);        //  64  65  66  67  68  69  70  71  72  73  74  75  76  77  78  79
    *i5 = _mm_unpackhi_epi32(t9, t11);        //  80  81  82  83  84  85  86  87  88  89  90  91  92  93  94  95
    *i6 = _mm_unpacklo_epi32(t13, t15);       //  96  97  98  99 100 101 102 103 104 105 106 107 108 109 110 111
    *i7 = _mm_unpackhi_epi32(t13, t15);       // 112 113 114 115 116 117 118 119 120 121 122 123 124 125 126 127
}

twp__INLINE static void twp__load_pixels_normal(uint8_t *plane0, uint8_t *plane1, int stride, int offset, int vert,
            __m128i *p3, __m128i *p2, __m128i *p1, __m128i *p0, __m128i *q0, __m128i *q1, __m128i *q2, __m128i *q3)
{
    int chroma = (plane0 && plane1);
    if (chroma) {
        // combine u and v into one. since chroma macroblocks are only 8x8,
        // we can fit both u and v into one sse register

        uint8_t *ptr = vert ? &plane0[offset - 4*stride] : &plane0[offset - 4];
        *p3 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *p2 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *p1 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *p0 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *q0 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *q1 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *q2 = _mm_loadl_epi64((__m128i *)ptr); ptr += stride;
        *q3 = _mm_loadl_epi64((__m128i *)ptr);

        ptr = vert ? &plane1[offset - 4*stride] : &plane1[offset - 4];
        *p3 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p3), (double *)ptr)); ptr += stride;
        *p2 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p2), (double *)ptr)); ptr += stride;
        *p1 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p1), (double *)ptr)); ptr += stride;
        *p0 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p0), (double *)ptr)); ptr += stride;
        *q0 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q0), (double *)ptr)); ptr += stride;
        *q1 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q1), (double *)ptr)); ptr += stride;
        *q2 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q2), (double *)ptr)); ptr += stride;
        *q3 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q3), (double *)ptr));

        if (!vert)
            twp__transpose_bytes_16x8(p3, p2, p1, p0, q0, q1, q2, q3);
    } else {
        if (vert) {
            uint8_t *ptr = &plane0[offset - 4*stride];
            *p3 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *p2 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *p1 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *p0 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *q0 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *q1 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *q2 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
            *q3 = _mm_loadu_si128((__m128i *)ptr);
        } else {
            uint8_t *ptr = &plane0[offset - 4];
            *p3 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *p3 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p3), (double *)ptr)); ptr += stride;
            *p2 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *p2 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p2), (double *)ptr)); ptr += stride;
            *p1 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *p1 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p1), (double *)ptr)); ptr += stride;
            *p0 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *p0 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*p0), (double *)ptr)); ptr += stride;
            *q0 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *q0 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q0), (double *)ptr)); ptr += stride;
            *q1 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *q1 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q1), (double *)ptr)); ptr += stride;
            *q2 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *q2 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q2), (double *)ptr)); ptr += stride;
            *q3 = _mm_loadl_epi64((__m128i *)ptr);                                      ptr += stride;
            *q3 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(*q3), (double *)ptr));

            twp__transpose_bytes_16x8(p3, p2, p1, p0, q0, q1, q2, q3);
        }
    }
}

twp__INLINE static void twp__store_pixels_normal(uint8_t *plane0, uint8_t *plane1, int stride, int offset, int vert,
                    __m128i p3, __m128i p2, __m128i p1, __m128i p0, __m128i q0, __m128i q1, __m128i q2, __m128i q3)
{
    // for some reason gcc requires _mm_storeh_pd to be 8-byte aligned, so we have to go through pain
    // to avoid ub. unfortunate
    double tmp;

    int chroma = (plane0 && plane1);
    if (chroma) {
        if (!vert)
            twp__transpose_bytes_16x8_reverse(&p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);

        uint8_t *ptr = vert ? &plane0[offset - 4*stride] : &plane0[offset - 4];
        _mm_storel_epi64((__m128i *)ptr, p3); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, p2); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, p1); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, p0); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, q0); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, q1); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, q2); ptr += stride;
        _mm_storel_epi64((__m128i *)ptr, q3);

        ptr = vert ? &plane1[offset - 4*stride] : &plane1[offset - 4];
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(p3)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(p2)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(p1)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(p0)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(q0)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(q1)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(q2)); memcpy(ptr, &tmp, 8); ptr += stride;
        _mm_storeh_pd(&tmp, _mm_castsi128_pd(q3)); memcpy(ptr, &tmp, 8);
    } else {
        if (vert) {
            uint8_t *ptr = &plane0[offset - 4*stride];
            _mm_storeu_si128((__m128i *)ptr, p3); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, p2); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, p1); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, p0); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, q0); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, q1); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, q2); ptr += stride;
            _mm_storeu_si128((__m128i *)ptr, q3);
        } else {
            twp__transpose_bytes_16x8_reverse(&p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
            uint8_t *ptr = &plane0[offset - 4];
            _mm_storel_epi64((__m128i *)ptr, p3);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(p3)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, p2);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(p2)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, p1);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(p1)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, p0);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(p0)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, q0);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(q0)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, q1);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(q1)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, q2);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(q2)); memcpy(ptr, &tmp, 8); ptr += stride;
            _mm_storel_epi64((__m128i *)ptr, q3);                            ptr += stride;
            _mm_storeh_pd(&tmp, _mm_castsi128_pd(q3)); memcpy(ptr, &tmp, 8);
        }
    }
}

twp__INLINE static __m128i twp__absdiff_u8(__m128i a, __m128i b)
{
    __m128i res = _mm_sub_epi8(_mm_max_epu8(a, b), _mm_min_epu8(a, b));
    return res;
}

twp__INLINE static void twp__compute_abs_diffs_normal(__m128i p3, __m128i p2, __m128i p1, __m128i p0,
                    __m128i q0, __m128i q1, __m128i q2, __m128i q3,
                    __m128i *ad_p3p2, __m128i *ad_p2p1, __m128i *ad_p1p0, __m128i *ad_q0q1,
                    __m128i *ad_q1q2, __m128i *ad_q2q3, __m128i *ad_p0q0, __m128i *ad_p1q1)
{
    *ad_p3p2 = twp__absdiff_u8(p3, p2);
    *ad_p2p1 = twp__absdiff_u8(p2, p1);
    *ad_p1p0 = twp__absdiff_u8(p1, p0);
    *ad_q0q1 = twp__absdiff_u8(q0, q1);
    *ad_q1q2 = twp__absdiff_u8(q1, q2);
    *ad_q2q3 = twp__absdiff_u8(q2, q3);
    *ad_p0q0 = twp__absdiff_u8(p0, q0);
    *ad_p1q1 = twp__absdiff_u8(p1, q1);
}

twp__INLINE static __m128i twp__flip_byte_msbs(__m128i bytes)
{
    bytes = _mm_add_epi8(bytes, _mm_set1_epi8(-128));
    return bytes;
}

twp__INLINE static void twp__flip_msbs_simple(__m128i *p1, __m128i *p0, __m128i *q0, __m128i *q1)
{
    *p1 = twp__flip_byte_msbs(*p1);
    *p0 = twp__flip_byte_msbs(*p0);
    *q0 = twp__flip_byte_msbs(*q0);
    *q1 = twp__flip_byte_msbs(*q1);
}

twp__INLINE static void twp__flip_msbs_normal(__m128i *p2, __m128i *p1, __m128i *p0, __m128i *q0, __m128i *q1, __m128i *q2)
{
    *p2 = twp__flip_byte_msbs(*p2);
    twp__flip_msbs_simple(p1, p0, q0, q1);
    *q2 = twp__flip_byte_msbs(*q2);
}

twp__INLINE static __m128i twp__threshold_simple(int edge_limit, __m128i ad_p0q0, __m128i ad_p1q1)
{
    __m128i xmm_edge_limit = _mm_set1_epi8((int8_t)edge_limit);
    __m128i simple0 = _mm_adds_epu8(ad_p0q0, ad_p0q0);
    __m128i simple1 = twp__logical_right_shift_bytes_by_1(ad_p1q1);
    __m128i simple2 = _mm_adds_epu8(simple0, simple1);
    __m128i simple3 = _mm_max_epu8(simple2, xmm_edge_limit);
    __m128i simple = _mm_cmpeq_epi8(simple3, xmm_edge_limit);
    return simple;
}

twp__INLINE static __m128i twp__threshold_normal(int interior_limit, int edge_limit,
                __m128i ad_p3p2, __m128i ad_p2p1, __m128i ad_p1p0, __m128i ad_q0q1,
                __m128i ad_q1q2, __m128i ad_q2q3, __m128i ad_p0q0, __m128i ad_p1q1)
{
    __m128i xmm_interior_limit = _mm_set1_epi8((int8_t)interior_limit);

    __m128i normal0 = _mm_cmpeq_epi8(_mm_max_epu8(ad_p3p2, xmm_interior_limit), xmm_interior_limit);
    __m128i normal1 = _mm_cmpeq_epi8(_mm_max_epu8(ad_p2p1, xmm_interior_limit), xmm_interior_limit);
    __m128i normal2 = _mm_cmpeq_epi8(_mm_max_epu8(ad_p1p0, xmm_interior_limit), xmm_interior_limit);
    __m128i normal3 = _mm_cmpeq_epi8(_mm_max_epu8(ad_q0q1, xmm_interior_limit), xmm_interior_limit);
    __m128i normal4 = _mm_cmpeq_epi8(_mm_max_epu8(ad_q1q2, xmm_interior_limit), xmm_interior_limit);
    __m128i normal5 = _mm_cmpeq_epi8(_mm_max_epu8(ad_q2q3, xmm_interior_limit), xmm_interior_limit);

    __m128i simple = twp__threshold_simple(edge_limit, ad_p0q0, ad_p1q1);

    __m128i th_mask;
    th_mask = _mm_and_si128(normal0, normal1);
    th_mask = _mm_and_si128(th_mask, normal2);
    th_mask = _mm_and_si128(th_mask, normal3);
    th_mask = _mm_and_si128(th_mask, normal4);
    th_mask = _mm_and_si128(th_mask, normal5);
    th_mask = _mm_and_si128(th_mask, simple);

    return th_mask;
}

twp__INLINE static __m128i twp__hev_mask(int hev_threshold, __m128i ad_p1p0, __m128i ad_q0q1)
{
    __m128i xmm_hev_threshold = _mm_set1_epi8((int8_t)(hev_threshold + 1));
    __m128i t0 = _mm_cmpeq_epi8(_mm_max_epu8(ad_p1p0, xmm_hev_threshold), ad_p1p0);
    __m128i t1 = _mm_cmpeq_epi8(_mm_max_epu8(ad_q0q1, xmm_hev_threshold), ad_q0q1);
    __m128i hev_mask = _mm_or_si128(t0, t1);
    return hev_mask;
}

twp__INLINE static __m128i twp__filter_common(__m128i p1, __m128i *p0, __m128i *q0, __m128i q1,
                                              __m128i use_outer_taps, __m128i write_mask)
{
    // this incurs a slight overhead for the normal mb and simple cases, because use_outer_taps is always 1
    // (assuming the compiler doesn't inline this function and realize that, which it might)
    __m128i a = _mm_and_si128(use_outer_taps, _mm_subs_epi8(p1, q1));
    __m128i d_q0p0 = _mm_subs_epi8(*q0, *p0);
    a = _mm_adds_epi8(a, d_q0p0);
    a = _mm_adds_epi8(a, d_q0p0);
    a = _mm_adds_epi8(a, d_q0p0);

    __m128i b = _mm_adds_epi8(a, _mm_set1_epi8(3));
    b = twp__arith_right_shift_bytes_by_3(b);

    __m128i c = _mm_adds_epi8(a, _mm_set1_epi8(4));
    c = twp__arith_right_shift_bytes_by_3(c);

    __m128i d_q0c = _mm_subs_epi8(*q0, c);
    __m128i s_p0b = _mm_adds_epi8(*p0, b);
    *q0 = _mm_or_si128(_mm_and_si128(write_mask, d_q0c), _mm_andnot_si128(write_mask, *q0));
    *p0 = _mm_or_si128(_mm_and_si128(write_mask, s_p0b), _mm_andnot_si128(write_mask, *p0));

    return c;
}

static void twp__normal_filter_mb(int edge_limit, int interior_limit, int hev_threshold,
                    uint8_t *plane0, uint8_t *plane1, int stride, int offset, int vert)
{
    __m128i p3, p2, p1, p0, q0, q1, q2, q3;
    __m128i ad_p3p2, ad_p2p1, ad_p1p0, ad_q0q1, ad_q1q2, ad_q2q3, ad_p0q0, ad_p1q1;

    twp__load_pixels_normal(plane0, plane1, stride, offset, vert, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
    twp__compute_abs_diffs_normal(p3, p2, p1, p0, q0, q1, q2, q3, &ad_p3p2, &ad_p2p1, &ad_p1p0, &ad_q0q1, &ad_q1q2, &ad_q2q3, &ad_p0q0, &ad_p1q1);
    twp__flip_msbs_normal(&p2, &p1, &p0, &q0, &q1, &q2); // need to convert to signed after we took the absolute differences because _mm_min_epu8 and _mm_min_epu8 take unsigned values

    __m128i th_mask = twp__threshold_normal(interior_limit, edge_limit, ad_p3p2, ad_p2p1, ad_p1p0, ad_q0q1, ad_q1q2, ad_q2q3, ad_p0q0, ad_p1q1);
    __m128i is_hev_mask = twp__hev_mask(hev_threshold, ad_p1p0, ad_q0q1);

    __m128i hev_and_th = _mm_and_si128(is_hev_mask, th_mask);
    __m128i hev_or_not_th = _mm_or_si128(is_hev_mask, _mm_andnot_si128(th_mask, _mm_set1_epi8(-1/*0xff*/)));

    twp__filter_common(p1, &p0, &q0, q1, _mm_set1_epi8(-1/*0xff*/), hev_and_th);

    __m128i w = _mm_subs_epi8(p1, q1);
    __m128i d_q0p0 = _mm_subs_epi8(q0, p0);
    w = _mm_adds_epi8(w, d_q0p0);
    w = _mm_adds_epi8(w, d_q0p0);
    w = _mm_adds_epi8(w, d_q0p0);

    // convert to 16-bit with sign extension
    __m128i tmp = _mm_cmplt_epi8(w, _mm_setzero_si128());
    __m128i w0 = _mm_unpacklo_epi8(w, tmp);
    __m128i w1 = _mm_unpackhi_epi8(w, tmp);

    // compute 9*w using a shift and an add instead of a mul, which should be slightly faster
    __m128i w9_0 = _mm_add_epi16(_mm_slli_epi16(w0, 3), w0);
    __m128i w9_1 = _mm_add_epi16(_mm_slli_epi16(w1, 3), w1);

    // we can compute 9*w, 18*w and 27*w by just using adds instead of muls,
    // since 18*w = 9*w + 9*w and 27*w = 18*w + 9*w
    __m128i c0 = _mm_add_epi16(w9_0, _mm_set1_epi16(63)); //  9*w + 63
    __m128i c1 = _mm_add_epi16(w9_1, _mm_set1_epi16(63));
    __m128i b0 = _mm_add_epi16(c0, w9_0);                 // 18*w + 63
    __m128i b1 = _mm_add_epi16(c1, w9_1);
    __m128i a0 = _mm_add_epi16(b0, w9_0);                 // 27*w + 63
    __m128i a1 = _mm_add_epi16(b1, w9_1);
    a0 = _mm_srai_epi16(a0, 7);
    a1 = _mm_srai_epi16(a1, 7);
    b0 = _mm_srai_epi16(b0, 7);
    b1 = _mm_srai_epi16(b1, 7);
    c0 = _mm_srai_epi16(c0, 7);
    c1 = _mm_srai_epi16(c1, 7);

    __m128i c = _mm_packs_epi16(c0, c1);
    __m128i b = _mm_packs_epi16(b0, b1);
    __m128i a = _mm_packs_epi16(a0, a1);

    __m128i d_q0a = _mm_subs_epi8(q0, a);
    __m128i s_p0a = _mm_adds_epi8(p0, a);
    q0 = _mm_or_si128(_mm_and_si128(hev_or_not_th, q0), _mm_andnot_si128(hev_or_not_th, d_q0a));
    p0 = _mm_or_si128(_mm_and_si128(hev_or_not_th, p0), _mm_andnot_si128(hev_or_not_th, s_p0a));

    __m128i d_q1b = _mm_subs_epi8(q1, b);
    __m128i s_p1b = _mm_adds_epi8(p1, b);
    q1 = _mm_or_si128(_mm_and_si128(hev_or_not_th, q1), _mm_andnot_si128(hev_or_not_th, d_q1b));
    p1 = _mm_or_si128(_mm_and_si128(hev_or_not_th, p1), _mm_andnot_si128(hev_or_not_th, s_p1b));

    __m128i d_q2c = _mm_subs_epi8(q2, c);
    __m128i s_p2c = _mm_adds_epi8(p2, c);
    q2 = _mm_or_si128(_mm_and_si128(hev_or_not_th, q2), _mm_andnot_si128(hev_or_not_th, d_q2c));
    p2 = _mm_or_si128(_mm_and_si128(hev_or_not_th, p2), _mm_andnot_si128(hev_or_not_th, s_p2c));

    twp__flip_msbs_normal(&p2, &p1, &p0, &q0, &q1, &q2);
    twp__store_pixels_normal(plane0, plane1, stride, offset, vert, p3, p2, p1, p0, q0, q1, q2, q3);
}

static void twp__normal_filter_sb(int edge_limit, int interior_limit, int hev_threshold,
                    uint8_t *plane0, uint8_t *plane1, int stride, int offset, int vert)
{
    int chroma = (plane0 && plane1);
    for (int j = 0; j < (chroma ? 1 : 3); ++j) {
        int sb_offset = offset;
        if (vert)
            sb_offset += (j+1)*4*stride;
        else
            sb_offset += (j+1)*4;

        __m128i p3, p2, p1, p0, q0, q1, q2, q3;
        __m128i ad_p3p2, ad_p2p1, ad_p1p0, ad_q0q1, ad_q1q2, ad_q2q3, ad_p0q0, ad_p1q1;

        twp__load_pixels_normal(plane0, plane1, stride, sb_offset, vert, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
        twp__compute_abs_diffs_normal(p3, p2, p1, p0, q0, q1, q2, q3, &ad_p3p2, &ad_p2p1, &ad_p1p0, &ad_q0q1, &ad_q1q2, &ad_q2q3, &ad_p0q0, &ad_p1q1);
        twp__flip_msbs_normal(&p2, &p1, &p0, &q0, &q1, &q2); // need to convert to signed after we took the absolute differences because _mm_min_epu8 and _mm_min_epu8 take unsigned values

        __m128i th_mask = twp__threshold_normal(interior_limit, edge_limit, ad_p3p2, ad_p2p1, ad_p1p0, ad_q0q1, ad_q1q2, ad_q2q3, ad_p0q0, ad_p1q1);
        __m128i is_hev_mask = twp__hev_mask(hev_threshold, ad_p1p0, ad_q0q1);
        __m128i not_hev_and_th = _mm_andnot_si128(is_hev_mask, th_mask);

        __m128i a = twp__filter_common(p1, &p0, &q0, q1, is_hev_mask, th_mask);

        // to compute (a + 1) >> 1 with "a" being signed, we do this:
        // 1. convert back to unsigned by flipping the msb
        // 2. use _mm_avg_epu8 to compute (a + 1) >> 1 on the unsigned value
        // 3. add 0b11000000 (-64) to flip the msb back and sign-extend; this works because if
        //    bit 7 (the previous msb) is 1 (meaning the value should be positive since it's flipped),
        //    after the add bit 7 and 8 will be 0 because of the carry. if bit 7 is 0, then we want
        //    bit 7 and 8 to be 1, and since there is no carry in that case, everything works
        a = twp__flip_byte_msbs(a);
        a = _mm_avg_epu8(a, _mm_setzero_si128());
        a = _mm_add_epi8(a, _mm_set1_epi8(-64));

        __m128i d_q1a = _mm_subs_epi8(q1, a);
        __m128i s_p1a = _mm_adds_epi8(p1, a);
        q1 = _mm_or_si128(_mm_and_si128(not_hev_and_th, d_q1a), _mm_andnot_si128(not_hev_and_th, q1));
        p1 = _mm_or_si128(_mm_and_si128(not_hev_and_th, s_p1a), _mm_andnot_si128(not_hev_and_th, p1));

        twp__flip_msbs_normal(&p2, &p1, &p0, &q0, &q1, &q2);
        twp__store_pixels_normal(plane0, plane1, stride, sb_offset, vert, p3, p2, p1, p0, q0, q1, q2, q3);
    }
}

static void twp__simple_filter_mb(int edge_limit, uint8_t *plane, int stride, int offset, int vert)
{
    __m128i p1, p0, q0, q1;
    if (vert) {
        uint8_t *ptr = &plane[offset - 2*stride];
        p1 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
        p0 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
        q0 = _mm_loadu_si128((__m128i *)ptr); ptr += stride;
        q1 = _mm_loadu_si128((__m128i *)ptr);
    } else {
        uint8_t *ptr = &plane[offset - 2];

        __m128i i0 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i0 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i0), (double *)ptr)); ptr += stride;
        __m128i i1 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i1 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i1), (double *)ptr)); ptr += stride;
        __m128i i2 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i2 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i2), (double *)ptr)); ptr += stride;
        __m128i i3 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i3 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i3), (double *)ptr)); ptr += stride;
        __m128i i4 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i4 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i4), (double *)ptr)); ptr += stride;
        __m128i i5 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i5 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i5), (double *)ptr)); ptr += stride;
        __m128i i6 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i6 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i6), (double *)ptr)); ptr += stride;
        __m128i i7 = _mm_loadl_epi64((__m128i *)ptr);                                     ptr += stride;
                i7 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(i7), (double *)ptr)); ptr += stride;

        // i0 =                                         0  1  2  3 .. .. .. ..  4  5  6  7 .. .. .. ..
        // i1 =                                         8  9 10 11 .. .. .. .. 12 13 14 15 .. .. .. ..
        // i2 =                                        16 17 18 19 .. .. .. .. 20 21 22 23 .. .. .. ..
        // i3 =                                        24 25 26 27 .. .. .. .. 28 29 30 31 .. .. .. ..
        // i4 =                                        32 33 34 35 .. .. .. .. 36 37 38 39 .. .. .. ..
        // i5 =                                        40 41 42 43 .. .. .. .. 44 45 46 47 .. .. .. ..
        // i6 =                                        48 49 50 51 .. .. .. .. 52 53 54 55 .. .. .. ..
        // i7 =                                        56 57 58 59 .. .. .. .. 60 61 62 63 .. .. .. ..

        __m128i t0 = _mm_unpacklo_epi8(i0, i1);     //  0  8  1  9  2 10  3 11 .. .. .. .. .. .. .. ..
        __m128i t1 = _mm_unpacklo_epi8(i2, i3);     // 16 24 17 25 18 26 19 27 .. .. .. .. .. .. .. ..
        __m128i t2 = _mm_unpacklo_epi8(i4, i5);     // 32 40 33 41 34 42 35 43 .. .. .. .. .. .. .. ..
        __m128i t3 = _mm_unpacklo_epi8(i6, i7);     // 48 56 49 57 50 58 51 59 .. .. .. .. .. .. .. ..
        __m128i t4 = _mm_unpackhi_epi8(i0, i1);     //  4 12  5 13  6 14  7 15 .. .. .. .. .. .. .. ..
        __m128i t5 = _mm_unpackhi_epi8(i2, i3);     // 20 28 21 29 22 30 23 31 .. .. .. .. .. .. .. ..
        __m128i t6 = _mm_unpackhi_epi8(i4, i5);     // 36 44 37 45 38 46 39 47 .. .. .. .. .. .. .. ..
        __m128i t7 = _mm_unpackhi_epi8(i6, i7);     // 52 60 53 61 54 62 55 63 .. .. .. .. .. .. .. ..

        __m128i t8 = _mm_unpacklo_epi8(t0, t4);     //  0  4  8 12  1  5  9 13  2  6 10 14  3  7 11 15
        __m128i t9 = _mm_unpacklo_epi8(t1, t5);     // 16 20 24 28 17 21 25 29 18 22 26 30 19 23 27 31
        __m128i t10 = _mm_unpacklo_epi8(t2, t6);    // 32 36 40 44 33 37 41 45 34 38 42 46 35 39 43 47
        __m128i t11 = _mm_unpacklo_epi8(t3, t7);    // 48 52 56 60 49 53 57 61 50 54 58 62 51 55 59 63

        __m128i t12 = _mm_unpacklo_epi32(t8, t9);   //  0  4  8 12 16 20 24 28  1  5  9 13 17 21 25 29
        __m128i t13 = _mm_unpacklo_epi32(t10, t11); // 32 36 40 44 48 52 56 60 33 37 41 45 49 53 57 61
        __m128i t14 = _mm_unpackhi_epi32(t8, t9);   //  2  6 10 14 18 22 26 30  3  7 11 15 19 23 27 31
        __m128i t15 = _mm_unpackhi_epi32(t10, t11); // 34 38 42 46 50 54 58 62 35 39 43 47 51 55 59 63

        p1 = _mm_unpacklo_epi64(t12, t13);          //  0  4  8 12 16 20 24 28 32 36 40 44 48 52 56 60
        p0 = _mm_unpackhi_epi64(t12, t13);          //  1  5  9 13 17 21 25 29 33 37 41 45 49 53 57 61
        q0 = _mm_unpacklo_epi64(t14, t15);          //  2  6 10 14 18 22 26 30 34 38 42 46 50 54 58 62
        q1 = _mm_unpackhi_epi64(t14, t15);          //  3  7 11 15 19 23 27 31 35 39 43 47 51 55 59 63
    }

    __m128i ad_p0q0 = twp__absdiff_u8(p0, q0);
    __m128i ad_p1q1 = twp__absdiff_u8(p1, q1);
    twp__flip_msbs_simple(&p1, &p0, &q0, &q1);
    __m128i threshold = twp__threshold_simple(edge_limit, ad_p0q0, ad_p1q1);
    twp__filter_common(p1, &p0, &q0, q1, _mm_set1_epi8(-1/*0xff*/), threshold);
    twp__flip_msbs_simple(&p1, &p0, &q0, &q1);

    if (vert) {
        uint8_t *ptr = &plane[offset - 1*stride];
        _mm_storeu_si128((__m128i *)ptr, p0); ptr += stride;
        _mm_storeu_si128((__m128i *)ptr, q0);
    } else {
        uint8_t *ptr = &plane[offset - 2];

        // p1 =                                     0  4  8 12 16 20 24 28 32 36 40 44 48 52 56 60
        // p0 =                                     1  5  9 13 17 21 25 29 33 37 41 45 49 53 57 61
        // q0 =                                     2  6 10 14 18 22 26 30 34 38 42 46 50 54 58 62
        // q1 =                                     3  7 11 15 19 23 27 31 35 39 43 47 51 55 59 63

        __m128i t0 = _mm_unpacklo_epi8(p1, p0); //  0  1  4  5  8  9 12 13 16 17 20 21 24 25 28 29
        __m128i t1 = _mm_unpacklo_epi8(q0, q1); //  2  3  6  7 10 11 14 15 18 19 22 23 26 27 30 31
        __m128i t2 = _mm_unpackhi_epi8(p1, p0); // 32 33 36 37 40 41 44 45 48 49 52 53 56 57 60 61
        __m128i t3 = _mm_unpackhi_epi8(q0, q1); // 34 35 38 39 42 43 46 47 50 51 54 55 58 59 62 63

        __m128i t4 = _mm_unpacklo_epi16(t0, t1); //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
        __m128i t5 = _mm_unpacklo_epi16(t2, t3); // 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47
        __m128i t6 = _mm_unpackhi_epi16(t0, t1); // 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
        __m128i t7 = _mm_unpackhi_epi16(t2, t3); // 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63

        int i;
        i = _mm_cvtsi128_si32(t4);                     memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t4,  4)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t4,  8)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t4, 12)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(t6);                     memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t6,  4)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t6,  8)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t6, 12)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(t5);                     memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t5,  4)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t5,  8)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t5, 12)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(t7);                     memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t7,  4)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t7,  8)); memcpy(ptr, &i, 4); ptr += stride;
        i = _mm_cvtsi128_si32(_mm_srli_si128(t7, 12)); memcpy(ptr, &i, 4);
    }
}

#endif

static void twp__simple_filter_sb(int edge_limit, uint8_t *plane, int stride, int mb_offset, int vert)
{
    for (int i = 0; i < 3; ++i) {
        int offset = mb_offset;
        if (vert)
            offset += (i+1)*4*stride;
        else
            offset += (i+1)*4;
        twp__simple_filter_mb(edge_limit, plane, stride, offset, vert);
    }
}

static void twp__do_loop_filtering(twp__vp8_data *data)
{
    for (int mb_y = 0; mb_y < data->mbs_per_col; ++mb_y) {
        for (int mb_x = 0; mb_x < data->mbs_per_row; ++mb_x) {
            int mb_i = mb_y*data->mbs_per_row + mb_x;
            twp__mb_info *mb_info = data->mb_infos + mb_i;
            int mb_luma_offset = twp__calc_mb_offset(mb_x, mb_y, data->luma_stride, 0);
            int mb_chroma_offset = twp__calc_mb_offset(mb_x, mb_y, data->chroma_stride, 1);

            int filter_level = twp__get_filter_level(data, mb_info);
            if (filter_level == 0) continue;
            int interior_limit = twp__get_interior_limit(filter_level, data->loop_filter_sharpness);
            int hev_threshold = twp__get_hev_threshold(filter_level);
            int mb_edge_limit, sb_edge_limit;
            twp__get_edge_limits(filter_level, interior_limit, &mb_edge_limit, &sb_edge_limit);

            if (data->loop_filter_type == twp__FILTER_NORMAL) {
                if (mb_x != 0) {
                    twp__normal_filter_mb(mb_edge_limit, interior_limit, hev_threshold, data->plane_y, NULL, data->luma_stride, mb_luma_offset, 0);
                    twp__normal_filter_mb(mb_edge_limit, interior_limit, hev_threshold, data->plane_u, data->plane_v, data->chroma_stride, mb_chroma_offset, 0);
                }
                if (!mb_info->skip_sb_filtering) {
                    twp__normal_filter_sb(sb_edge_limit, interior_limit, hev_threshold, data->plane_y, NULL, data->luma_stride, mb_luma_offset, 0);
                    twp__normal_filter_sb(sb_edge_limit, interior_limit, hev_threshold, data->plane_u, data->plane_v, data->chroma_stride, mb_chroma_offset, 0);
                }

                if (mb_y != 0) {
                    twp__normal_filter_mb(mb_edge_limit, interior_limit, hev_threshold, data->plane_y, NULL, data->luma_stride, mb_luma_offset, 1);
                    twp__normal_filter_mb(mb_edge_limit, interior_limit, hev_threshold, data->plane_u, data->plane_v, data->chroma_stride, mb_chroma_offset, 1);
                }
                if (!mb_info->skip_sb_filtering) {
                    twp__normal_filter_sb(sb_edge_limit, interior_limit, hev_threshold, data->plane_y, NULL, data->luma_stride, mb_luma_offset, 1);
                    twp__normal_filter_sb(sb_edge_limit, interior_limit, hev_threshold, data->plane_u, data->plane_v, data->chroma_stride, mb_chroma_offset, 1);
                }
            } else if (data->loop_filter_type == twp__FILTER_SIMPLE) {
                if (mb_x != 0)
                    twp__simple_filter_mb(mb_edge_limit, data->plane_y, data->luma_stride, mb_luma_offset, 0);
                if (!mb_info->skip_sb_filtering)
                    twp__simple_filter_sb(sb_edge_limit, data->plane_y, data->luma_stride, mb_luma_offset, 0);
                if (mb_y != 0)
                    twp__simple_filter_mb(mb_edge_limit, data->plane_y, data->luma_stride, mb_luma_offset, 1);
                if (!mb_info->skip_sb_filtering)
                    twp__simple_filter_sb(sb_edge_limit, data->plane_y, data->luma_stride, mb_luma_offset, 1);
            } else {
                twp__assert(0);
            }
        }
    }
}

static uint8_t *twp_yuv_to_rgb(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v,
            int img_width, int img_height, int luma_stride, int chroma_stride, int rgba)
{
    // https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion

    // this uses 16.16 fixed point and takes advantage of _mm_mulhi_epu16. however, that means that
    // we lose the decimal part immediately after the multiply, so we lose some information there.
    // seems good enough, though. i can't really perceive any difference compared to a "perfect"
    // floating point implementation

    int comp = rgba ? 4 : 3;
    uint8_t *mem = (uint8_t *)malloc(img_width * img_height * comp);

    uint8_t *src_y = plane_y + luma_stride + 1;
    uint8_t *src_u = plane_u + chroma_stride + 1;
    uint8_t *src_v = plane_v + chroma_stride + 1;
    uint8_t *dst = mem;

    for (int row = 0; row < img_height; ++row) {
        int col = 0;

#ifdef twp__SSE2
        // todo: i don't know if there is a good way to convert to rgb without pshufb, which we can't use
        // because we are limited to sse2. so yuv->rgb conversion just always uses the non-simd path
        for (; rgba && col+8 <= img_width; col += 8) {
            // load and upsample u, v
            __m128i y = _mm_loadl_epi64((__m128i *)(src_y + col));
            int i;
            memcpy(&i, src_u + col/2, 4);
            __m128i u = _mm_cvtsi32_si128(i);
            u = _mm_unpacklo_epi8(u, u);
            memcpy(&i, src_v + col/2, 4);
            __m128i v = _mm_cvtsi32_si128(i);
            v = _mm_unpacklo_epi8(v, v);
            __m128i a = _mm_set1_epi16(255);

            // convert to 16-bit and shift left by 8
            __m128i yw = _mm_unpacklo_epi8(_mm_setzero_si128(), y);
            __m128i uw = _mm_unpacklo_epi8(_mm_setzero_si128(), u);
            __m128i vw = _mm_unpacklo_epi8(_mm_setzero_si128(), v);

            // do the actual computation
            __m128i t0 = _mm_mulhi_epu16(yw, _mm_set1_epi16((short)(1.164*256+0.5)));
            __m128i t1 = _mm_mulhi_epu16(vw, _mm_set1_epi16((short)(1.596*256+0.5)));
            __m128i t2 = _mm_add_epi16(t0, t1);
            __m128i  r = _mm_sub_epi16(t2, _mm_set1_epi16(223));

            __m128i t3 = _mm_mulhi_epu16(uw, _mm_set1_epi16((short)(0.392*256+0.5)));
            __m128i t4 = _mm_mulhi_epu16(vw, _mm_set1_epi16((short)(0.813*256+0.5)));
            __m128i t5 = _mm_sub_epi16(t0, t3);
            __m128i t6 = _mm_sub_epi16(t5, t4);
            __m128i  g = _mm_add_epi16(t6, _mm_set1_epi16(135)); // 135 seems to work better than 136

            __m128i t7 = _mm_mulhi_epu16(uw, _mm_set1_epi16((short)(2.017*256+0.5)));
            __m128i t8 = _mm_add_epi16(t0, t7);
            __m128i  b = _mm_sub_epi16(t8, _mm_set1_epi16(277));

            // convert back to 8-bit and transpose
            __m128i rb = _mm_packus_epi16(r, b);        // rrrrrrrrbbbbbbbb
            __m128i ga = _mm_packus_epi16(g, a);        // ggggggggaaaaaaaa
            __m128i rg = _mm_unpacklo_epi8(rb, ga);     // rgrgrgrgrgrgrgrg
            __m128i ba = _mm_unpackhi_epi8(rb, ga);     // babababababababa
            __m128i rgba0 = _mm_unpacklo_epi16(rg, ba); // rgbargbargbargba
            __m128i rgba1 = _mm_unpackhi_epi16(rg, ba); // rgbargbargbargba

            // store
            _mm_storeu_si128((__m128i *)(dst +  0), rgba0);
            _mm_storeu_si128((__m128i *)(dst + 16), rgba1);
            dst += 32;
        }
#endif

        for (; col < img_width; ++col) {
            int y = (int)src_y[col];
            int u = (int)src_u[col>>1];
            int v = (int)src_v[col>>1];

            #define twp__FPMUL(c, x) (((int)((c)*256+0.5) * ((x)<<8)) >> 16)
            int ytmp = twp__FPMUL(1.164, y);
            int r = ytmp + twp__FPMUL(1.596, v) - 223;
            int g = ytmp - twp__FPMUL(0.392, u) - twp__FPMUL(0.813, v) + 135;
            int b = ytmp + twp__FPMUL(2.017, u) - 277;
            #undef twp__FPMUL

            *dst++ = (uint8_t)twp__clamp(r, 0, 255);
            *dst++ = (uint8_t)twp__clamp(g, 0, 255);
            *dst++ = (uint8_t)twp__clamp(b, 0, 255);
            if (rgba) *dst++ = 255;
        }

        src_y += luma_stride;
        if (row & 1) {
            src_u += chroma_stride;
            src_v += chroma_stride;
        }
    }

    return mem;
}

static void twp__vp8_init(twp__vp8_data *data)
{
    memset(data, 0, sizeof(*data));

    twp__assert(sizeof(twp__default_coeff_probs) == sizeof(twp__default_coeff_probs));
    memcpy(data->coeff_probs, twp__default_coeff_probs, sizeof(twp__default_coeff_probs));

    for (int i = 0; i < twp__arrlen(data->segment_id_tree_probs); ++i)
        data->segment_id_tree_probs[i] = 255;
}

static uint8_t *twp__read_vp8(void *raw_bytes, int num_bytes, int *width, int *height, twp_format format, twp_flags flags)
{
    uint8_t *result = NULL;

    twp__vp8_data data;
    twp__vp8_init(&data);

    if (!twp__read_vp8_header(&data, (uint8_t *)raw_bytes, num_bytes)) goto end;
    if (!twp__read_yuv_data(&data, format)) goto end;

    if (!(flags & twp_FLAG_SKIP_LOOP_FILTER))
        twp__do_loop_filtering(&data);

    *width = data.width;
    *height = data.height;

    if (format == twp_FORMAT_YUV || format == twp_FORMAT_YUVA) {
        result = data.plane_y;
        goto end;
    }

    result = twp_yuv_to_rgb(data.plane_y, data.plane_u, data.plane_v, data.width,
                            data.height, data.luma_stride, data.chroma_stride,
                            format == twp_FORMAT_RGBA);

end:;

    if (format != twp_FORMAT_YUV && format != twp_FORMAT_YUVA)
        free(data.plane_y);
    free(data.mb_infos);

    return result;
}

static int twp__read_alpha(void *data, int data_len, int width, int height, unsigned char *pix, int yuva)
{
    unsigned char *u8data = (unsigned char *)data;

    int hdr = *u8data;
    // int pp = (hdr >> 4) & 0x3; // we don't do anything with this
    int filter = (hdr >> 2) & 0x3;
    int compression = hdr & 0x3;

    if (compression != 0 && compression != 1)
        return 0;

    int src_pixsize;
    unsigned char *buf;
    unsigned char *src;
    if (compression == 0) {
        if (data_len-1 != width*height)
            return 0;
        buf = NULL;
        src_pixsize = 1;
        src = u8data + 1;
    } else {
        buf = twp__read_vp8l(u8data + 1, data_len-1, &width, &height, twp_FORMAT_RGBA, 1);
        if (!buf)
            return 0;
        src_pixsize = 4;
        src = buf + 1;
    }

    // this is a really slow way of doing this, but it seems this is never a hotspot, so i don't care

    // when yuva = 1, we don't have an rgba image in which we want to fill in the alpha channel values.
    // instead, we just want to directly return the planar alpha channel
    int dst_pixsize;
    int dst_stride;
    unsigned char *dst;
    if (yuva) {
        twp_unpack_yuv(pix, width, height, NULL, NULL, NULL, &dst, NULL, NULL, &dst_stride);
        dst_pixsize = 1;
    } else {
        dst_stride = width * 4;
        dst = pix + 3;
        dst_pixsize = 4;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int A, B, C;
            if (x > 0 && y > 0) {
                A = dst[-dst_pixsize];
                B = dst[-dst_stride];
                C = dst[-dst_stride - dst_pixsize];
            } else if (x > 0) {
                A = dst[-dst_pixsize];
                B = A;
                C = A;
            } else if (y > 0) {
                B = dst[-dst_stride];
                A = B;
                C = B;
            } else {
                A = 0;
                B = 0;
                C = 0;
            }

            int pred;
            switch (filter) {
                case 0: pred = 0; break;
                case 1: pred = A; break;
                case 2: pred = B; break;
                case 3: pred = twp__clamp(A + B - C, 0, 255); break;
                default: pred = 0; twp__assert(0); break;
            }

            *dst = (uint8_t)(*src + pred);
            dst += dst_pixsize;
            src += src_pixsize;
        }
    }

    free(buf);
    return 1;
}

static int twp__read_chunks(unsigned char *u8data, int data_len, twp__chunk_table *table)
{
    // the spec says chunks should be in a specific order, but we don't really care,
    // so we don't bother checking

    memset(table, 0, sizeof(*table));

    // this needs 2 variables because first can be NULL
    int first_iter = 1;
    twp__chunk *first = NULL;

    unsigned char *chunk = u8data + 12;
    while (chunk+8 <= u8data+data_len) {
        uint32_t size = (uint32_t)chunk[4] | ((uint32_t)chunk[5] << 8) | ((uint32_t)chunk[6] << 16) | ((uint32_t)chunk[7] << 24);
        if (size > INT32_MAX) return 0;

        unsigned char *chunk_data = chunk + 8;
        if (chunk_data+size > u8data+data_len) return 0;

        twp__chunk *c = NULL;
        if (twp__check_fourcc(chunk, "ANIM")) return 0; // not supported
        else if (twp__check_fourcc(chunk, "VP8X")) c = &table->VP8X;
        else if (twp__check_fourcc(chunk, "VP8L")) c = &table->VP8L;
        else if (twp__check_fourcc(chunk, "VP8 ")) c = &table->VP8;
        else if (twp__check_fourcc(chunk, "ALPH")) c = &table->ALPH;

        if (c) {
            if (c->data) return 0; // found same chunk twice
            c->data = chunk_data;
            c->size = (int)size;
        }

        if (first_iter) {
            first = c;
            first_iter = 0;
        }

        chunk += 8 + size;
        if (size & 1) ++chunk; // if chunk size is odd, a single padding byte is added
    }

    // validate

    // while we genrally do not care about chunk order, twp_get_info() requires that the first chunk
    // is correct, since it only reads that one. so to be consistent, we enforce this here as well
    if (first != &table->VP8X && first != &table->VP8L && first != &table->VP8)
        return 0;

    if (table->ALPH.data && !table->VP8X.data) return 0; // alpha chunk is only allowed in the extended format
    if (!table->VP8L.data && !table->VP8.data) return 0; // must have either lossless or lossy chunk
    if (table->VP8L.data && table->ALPH.data)  return 0; // can't have an alpha chunk with a lossless chunk
    if (table->VP8L.data && table->VP8.data)   return 0; // can't have both lossless and lossy chunks

    return 1;
}

twp__STORAGE unsigned char *twp_read_from_memory(void *data, int data_len, int *width, int *height, twp_format format, twp_flags flags)
{
    unsigned char *u8data = (unsigned char *)data;

    if (data_len < twp__MIN_FILE_SIZE) return NULL;
    if (!twp__check_fourcc(u8data, "RIFF")) return NULL;
    if (!twp__check_fourcc(u8data + 8, "WEBP")) return NULL;

    uint32_t hdr_file_size = (uint32_t)u8data[4] | ((uint32_t)u8data[5] << 8) | ((uint32_t)u8data[6] << 16) | ((uint32_t)u8data[7] << 24);
    if ((uint64_t)hdr_file_size+8 > INT32_MAX) return NULL; // not supported, we use ints everywhere
    if ((int)hdr_file_size+8 > data_len) return NULL; // the spec says we "may" parse files that have useless trailing bytes, so we do > insted of !=

    twp__chunk_table chunks;
    if (!twp__read_chunks(u8data, data_len, &chunks))
        return 0;

    unsigned char *result = NULL;
    if (chunks.VP8L.data)
        result = twp__read_vp8l(chunks.VP8L.data, chunks.VP8L.size, width, height, format, 0);
    else if (chunks.VP8.data)
        result = twp__read_vp8(chunks.VP8.data, chunks.VP8.size, width, height, format, flags);
    else
        twp__assert(0);

    if (chunks.ALPH.data && (format == twp_FORMAT_RGBA || format == twp_FORMAT_YUVA)) {
        if (!twp__read_alpha(chunks.ALPH.data, chunks.ALPH.size, *width, *height, result, format == twp_FORMAT_YUVA)) {
            free(result);
            return NULL;
        }
    }

    if (format == twp_FORMAT_YUVA && chunks.VP8.data && !chunks.ALPH.data) {
        // fill with 255 in case we want yuva but didn't have an alpha channel
        unsigned char *a;
        twp_unpack_yuv(result, *width, *height, NULL, NULL, NULL, &a, NULL, NULL, NULL);
        memset(a, 255, *width * *height);
    }

    return result;
}

twp__STORAGE void twp_unpack_yuv(unsigned char *ptr, int width, int height,
                    unsigned char **y, unsigned char **u, unsigned char **v, unsigned char **a,
                    int *luma_stride, int *chroma_stride, int *alpha_stride)
{
    int luma_width = twp__div_round_up(width, 16) * 16;
    int luma_height = twp__div_round_up(height, 16) * 16;
    int chroma_width = luma_width / 2;
    int chroma_height = luma_height / 2;
    int luma_stride_ = 1 + luma_width + 4;
    int chroma_stride_ = 1 + chroma_width;
    if (luma_stride) *luma_stride = luma_stride_;
    if (chroma_stride) *chroma_stride = chroma_stride_;
    if (alpha_stride) *alpha_stride = width;

    int y_bufsize = luma_stride_ * (luma_height+1);
    int uv_bufsize = chroma_stride_ * (chroma_height+1);
    if (y) *y = ptr + luma_stride_ + 1;
    if (u) *u = ptr + y_bufsize + chroma_stride_ + 1;
    if (v) *v = ptr + y_bufsize + uv_bufsize + chroma_stride_ + 1;
    if (a) *a = ptr + y_bufsize + uv_bufsize*2;
}

#ifdef _WIN32
#ifdef __cplusplus
extern "C"
#endif
__declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned int cp, unsigned long flags, const char *str, int cbmb, wchar_t *widestr, int cchwide);
#endif

static FILE *twp__fopen(const char *file_path)
{
#if defined(_WIN32)
    wchar_t wide_file_path[4096];
    if (!MultiByteToWideChar(65001 /* UTF8 */, 0, file_path, -1, wide_file_path, sizeof(wide_file_path) / sizeof(*wide_file_path)))
        return NULL;
#if defined(_MSC_VER) && _MSC_VER >= 1400
    FILE *f = NULL;
    if (_wfopen_s(&f, wide_file_path, L"rb") != 0) return NULL;
    else return f;
#else
    return _wfopen(wide_file_path, L"rb");
#endif
#else
    return fopen(file_path, "rb");
#endif
}

static unsigned char *twp__read_entire_file(const char *file_path, int *size)
{
    FILE *f = twp__fopen(file_path);
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *mem = malloc(*size);
    fread(mem, 1, *size, f);
    fclose(f);

    return (unsigned char *)mem;
}

twp__STORAGE int twp_get_info_from_memory(void *data, int data_len, int *width, int *height, int *lossless, int *alpha)
{
    if (data_len < 16)
        return 0;

    unsigned char *u8data = (unsigned char *)data;

    if (twp__check_fourcc(u8data + 12, "VP8L")) {
        if (data_len < 25)
            return 0;

        if (lossless) *lossless = 1;
        if (width) *width = ((int)u8data[21] | (((int)u8data[22] & 0x3f) << 8)) + 1;
        if (height) *height = (((int)u8data[22] >> 6) | ((int)u8data[23] << 2) | (((int)u8data[24] & 0xf) << 10)) + 1;
        if (alpha) *alpha = !!(u8data[24] & 0x10);
    } else if (twp__check_fourcc(u8data + 12, "VP8 ")) {
        if (data_len < 30)
            return 0;

        if (lossless) *lossless = 0;
        if (width) *width = (int)u8data[26] | (((int)u8data[27] & 0x3f) << 8);
        if (height) *height = (int)u8data[28] | (((int)u8data[29] & 0x3f) << 8);
        if (alpha) *alpha = 0;
    } else if (twp__check_fourcc(u8data + 12, "VP8X")) {
        if (data_len < 30)
            return 0;

        if (lossless) *lossless = 0;
        if (width) *width = ((int)u8data[24] | ((int)u8data[25] << 8) | ((int)u8data[26] << 16)) + 1;
        if (height) *height = ((int)u8data[27] | ((int)u8data[28] << 8) | ((int)u8data[29] << 16)) + 1;
        if (alpha) *alpha = !!(u8data[20] & 0x10);

        if (u8data[20] & 0x2) // we don't support animation
            return 0;

        if (*width > 16383 || *height > 16383) // bigger sizes only possible for animations
            return 0;
    } else {
        return 0;
    }

    return 1;
}

twp__STORAGE int twp_get_info(const char *file_path, int *width, int *height, int *lossless, int *alpha)
{
    int ok = 0;

    FILE *f = twp__fopen(file_path);
    if (!f)
        return 0;

    unsigned char buf[30];
    int buflen = 0;

    if (fread(buf, 1, 16, f) != 16)
        goto end;

    if (twp__check_fourcc(buf + 12, "VP8L")) {
        if (fread(buf+16, 1, 9, f) != 9)
            goto end;
        buflen = 25;
    } else if (twp__check_fourcc(buf + 12, "VP8 ") || twp__check_fourcc(buf + 12, "VP8X")) {
        if (fread(buf+16, 1, 14, f) != 14)
            goto end;
        buflen = 30;
    } else {
        goto end;
    }
    twp__assert(twp__arrlen(buf) >= buflen);

    ok = twp_get_info_from_memory(buf, buflen, width, height, lossless, alpha);

end:
    fclose(f);
    return ok;
}

twp__STORAGE unsigned char *twp_read(const char *file_path, int *width, int *height, twp_format format, twp_flags flags)
{
    int size = 0;
    unsigned char *data = twp__read_entire_file(file_path, &size);
    if (!data) return NULL;
    unsigned char *result = twp_read_from_memory(data, size, width, height, format, flags);
    free(data);
    return result;
}

#endif
