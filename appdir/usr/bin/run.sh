#!/bin/bash

# Set the LD_LIBRARY_PATH to ensure system FFmpeg libraries are used
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH

# Find the system-installed libavcodec, libavformat, and other FFmpeg libraries
LIBAVCODEC=$(find /usr/lib /usr/local/lib -name "libavcodec.so*" 2>/dev/null | head -n 1)
LIBAVFORMAT=$(find /usr/lib /usr/local/lib -name "libavformat.so*" 2>/dev/null | head -n 1)
LIBAVUTIL=$(find /usr/lib /usr/local/lib -name "libavutil.so*" 2>/dev/null | head -n 1)
LIBSWRESAMPLE=$(find /usr/lib /usr/local/lib -name "libswresample.so*" 2>/dev/null | head -n 1)
LIBSWSCALE=$(find /usr/lib /usr/local/lib -name "libswscale.so*" 2>/dev/null | head -n 1)

# If any of the required FFmpeg libraries are not found, exit with an error
if [ -z "$LIBAVCODEC" ] || [ -z "$LIBAVFORMAT" ] || [ -z "$LIBAVUTIL" ]; then
    echo "FFmpeg libraries not found. Please install FFmpeg on your system."
    exit 1
fi

# Add the libraries to LD_PRELOAD dynamically
export LD_PRELOAD="$LIBAVCODEC:$LIBAVFORMAT:$LIBAVUTIL:$LIBSWRESAMPLE:$LIBSWSCALE"

# Execute the main application
exec "$@"