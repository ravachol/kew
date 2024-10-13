
/*
This implements a data source that handles m4a streams via minimp4 and faad2

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/

#define MINIMP4_IMPLEMENTATION
#include "../include/minimp4/minimp4.h"

#include <miniaudio.h>
#ifdef USE_FAAD
#include "m4a.h"
#endif
