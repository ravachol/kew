# tiny_webp.h

Reasonably tiny, single-header WebP library in C99.

## Examples
The API is explained in more detail below, but here are some quick examples:

Load RGB/RGBA:
```c
// If you already have the file in memory, use twp_read_from_memory() instead.
int width, height;
unsigned char *data = twp_read("path/to/file.webp",
                               &width, &height,
                               twp_FORMAT_RGBA, // or twp_FORMAT_RGB
                               0); // no flags
if (data) { } // Do something with the data
free(data);
```

Get information about a .webp file:
```c
// If you already have the file in memory, use twp_get_info_from_memory() instead.
int width, height, lossless, alpha;
int ok = twp_get_info("path/to/file.webp", &width, &height, &lossless, &alpha);
if (ok) { } // Do something with the info
```

Load YUV/YUVA:
```c
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
```

## Compilation
Copy tiny_webp.h into your project, then `#define twp_IMPLEMENTATION` in exactly one C/C++ file that #includes tiny_webp.h. For example:
```c
#include <stdio.h>
#include <stdlib.h>
#define twp_IMPLEMENTATION
#include "tiny_webp.h"
```

You can also do the `#define twp_IMPLEMENTATION` part at the end of a file. That way you don't see all the private symbols in your autocomplete:
```c
#include "tiny_webp.h"

int main()
{
    ...
}

#define twp_IMPLEMENTATION
#include "tiny_webp.h"
```

Alternatively, you could also just create a .c file that is empty except for the #define and the #include.

## API
```c
unsigned char *twp_read(const char *file_path, int *width, int *height,
                        twp_format format, twp_flags flags)
```
Loads a .webp image from a file. You must free the result.

Returns NULL on error.

Formats:
- `twp_FORMAT_RGBA`
- `twp_FORMAT_RGB`
- `twp_FORMAT_YUV`
- `twp_FORMAT_YUVA`

YUV and YUVA will return NULL for lossless images. Use `twp_get_info()` to first check if you have a lossless or a lossy image.

For YUV and YUVA, you must call `twp_unpack_yuv()` on the result.

Flags:
- `twp_FLAG_SKIP_LOOP_FILTER`: Skips loop filtering, which saves some decoding time, but slightly lowers quality.

---

<br>

```c
unsigned char *twp_read_from_memory(void *data, int data_len, int *width, int *height,
                                    twp_format format, twp_flags flags)
```

The same as `twp_read()`, except it reads from memory instead.

---

<br>

```c
void twp_unpack_yuv(unsigned char *ptr, int width, int height,
                    unsigned char **y, unsigned char **u, unsigned char **v, unsigned char **a,
                    int *luma_stride, int *chroma_stride, int *alpha_stride)
```

If you loaded an image and requested either YUV or YUVA as the format, you must call this function to unpack the returned pointer into the individual planes and get the strides.

Do not free the returned Y, U, V, A pointers. Just free the original pointer returned from `twp_read()`. Y, U, V and A will be valid for as long as the original pointer is valid.

You can pass NULL if you don't care about a certain plane. For example, if you requested
format YUV, there is no alpha, so pass NULL for the `a` and `alpha_stride` parameters.

---

<br>

```c
int twp_get_info(const char *file_path, int *width, int *height, int *lossless, int *alpha)
```

Get information about a .webp file.

You can pass NULL if you don't care about something.

Returns 0 on error and 1 on success.

---

<br>

```c
int twp_get_info_from_memory(void *data, int data_len,
                             int *width, int *height, int *lossless, int *alpha)
```

The same as `twp_get_info()`, except it reads from memory instead.

## Compile Time Options
Simply `#define` any of these. `twp_STATIC` needs to be seen by the header and the implementation. For all the others, only the implementation needs to see the `#define`. I recommend just putting them at the top of tiny_webp.h.

- `twp_STATIC`: Define extern function to be static instead.
- `twp_NO_SIMD`: Disable all SIMD optimizations.
- `twp_FORCE_SSE2`: If for some reason SSE2 support wasn't correctly detected, you can force-enable it.
- `twp_SIGNED_RIGHT_SHIFT_FIX`: This library heavily relies on right-shifting negative integers to compile to arithmetic shifts. Enable this option if your compiler does not guarantee this (which is unlikely, all major compilers do).

## Current Limitations
- Probably not as fast as libwebp
- No encoding
- No animations
- SIMD optimizations are SSE2 only
- License is not 0BSD, which would be my preference (but that's not my fault, see the next section)

## License
As far as I understand, it's actually impossible to release an implementation of the WebP spec under anything more permissive than BSD3. The reason for this is that the spec uses C code everywhere, meaning if you read the code and then write an implementation, you have created a derivative work. If the spec was written purely in English, this would not be a problem, because implementing something described in English does not count as a derivative work of that description. The spec further says that "the bitstream is defined by the reference source code and not this narrative." So, really, the source code is the spec, and therefore any implementation of the spec is a derivative work.

It seems to me, then, that a bunch of WebP/WebM/VP8 implementations floating around the web are quite openly violating the license.

If Google could re-license the spec to 0BSD or something, that would be great.
