CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config

# To enable debugging, run:
# make DEBUG=1
# To disable DBUS notifications, run:
# make USE_DBUS=0
# To disable faad2, run:
# make USE_FAAD=0

# Detect system and architecture
UNAME_S := $(shell uname -s)
ARCH := $(shell uname -m)

# Set kew version
KEW_VERSION ?= $(shell git describe --tags --dirty --always)

  # Check if we're in Termux environment
ifneq ($(wildcard /data/data/com.termux/files/usr),)
  # Termux environment
  COMMONFLAGS += -D__ANDROID__
  IS_ANDROID := 1
endif

# Default USE_DBUS to auto-detect if not set by user
ifeq ($(origin USE_DBUS), undefined)
  ifeq ($(UNAME_S), Darwin)
    USE_DBUS = 0
  else ifeq ($(IS_ANDROID),1)
    USE_DBUS = 0
  else
    USE_DBUS = 1
  endif
endif

# Default USE_MACOS_MEDIA to auto-detect if not set by user
ifeq ($(origin USE_MACOS_MEDIA), undefined)
  ifeq ($(UNAME_S), Darwin)
    USE_MACOS_MEDIA = 1
  else
    USE_MACOS_MEDIA = 0
  endif
endif

PREFIX    ?= /usr/local

ifeq ($(UNAME_S),Darwin)
    ifeq ($(ARCH),arm64)
        PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:$(PKG_CONFIG_PATH)
    else
        PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)
    endif

    export PKG_CONFIG_PATH
endif

# Default USE_FAAD to auto-detect if not set by user
ifeq ($(origin USE_FAAD), undefined)

  # Check if we're in Termux environment
  ifneq ($(wildcard /data/data/com.termux/files/usr),)
    # Termux environment - check common installation paths
    USE_FAAD = $(shell [ -f "$(PREFIX)/lib/libfaad.so" ] || \
                       [ -f "$(PREFIX)/lib/libfaad2.so" ] || \
                       [ -f "$(PREFIX)/local/lib/libfaad.so" ] || \
                       [ -f "$(PREFIX)/local/lib/libfaad2.so" ] || \
                       [ -f "/data/data/com.termux/files/usr/lib/libfaad.so" ] || \
                       [ -f "/data/data/com.termux/files/usr/bin/faad" ] || \
                       [ -f "/data/data/com.termux/files/usr/lib/libfaad2.so" ] || \
                       [ -f "/data/data/com.termux/files/usr/local/lib/libfaad.so" ] || \
                       [ -f "/data/data/com.termux/files/usr/local/lib/libfaad2.so" ] && echo 1 || echo 0)
  else
    # Non-Android build - try pkg-config first
    USE_FAAD = $(shell $(PKG_CONFIG) --exists faad && echo 1 || echo 0)

    ifeq ($(USE_FAAD), 0)
        # If pkg-config fails, try to find libfaad dynamically in common paths
        USE_FAAD = $(shell [ -f /usr/lib/libfaad.so ] || [ -f /usr/lib64/libfaad.so ] || [ -f /usr/lib64/libfaad2.so ] || \
                        [ -f /usr/bin/faad ] || [ -f /usr/local/lib/libfaad.so ] || \
                        [ -f /opt/local/lib/libfaad.so ] || [ -f /opt/homebrew/lib/libfaad.dylib ] || \
                        [ -f /opt/homebrew/opt/faad2/lib/libfaad.dylib ] || \
                         [ -f /usr/local/lib/libfaad.dylib ] || [ -f /lib/x86_64-linux-gnu/libfaad.so.2 ] && echo 1 || echo 0)
    endif
  endif
endif

LOCAL_INC = \
    -Isrc \
    -Iinclude \
    -Iinclude/minimp4 \
    -Iinclude/stb_image \
    -Iinclude/miniaudio \
    -Iinclude/nestegg

ifeq ($(UNAME_S),Darwin)
    PKG_LIBS = gio-2.0 chafa fftw3f opus opusfile vorbis vorbisfile ogg glib-2.0 taglib gdk-pixbuf-2.0
else
    PKG_LIBS = gio-2.0 chafa fftw3f opus opusfile vorbis vorbisfile ogg glib-2.0 taglib
endif

PKG_CFLAGS  = $(shell $(PKG_CONFIG) --cflags $(PKG_LIBS))
PKG_LDFLAGS = $(shell $(PKG_CONFIG) --libs $(PKG_LIBS))

COMMONFLAGS = $(LOCAL_INC) $(PKG_CFLAGS)

ifeq ($(DEBUG), 1)
COMMONFLAGS += -g -DDEBUG
else
COMMONFLAGS += -O2
endif

ifneq ($(strip $(KEW_VERSION)),)
  COMMONFLAGS += -DKEW_VERSION=\"$(KEW_VERSION)\"
endif

COMMONFLAGS += -DMA_NO_AAUDIO
COMMONFLAGS += -fstack-protector-strong -Wformat -Wno-format-security -fPIE -D_FORTIFY_SOURCE=2
COMMONFLAGS += -Wall -Wextra -Wpointer-arith

CFLAGS = $(COMMONFLAGS)

# Compiler flags for C++ code
CXXFLAGS = $(COMMONFLAGS) -std=c++11

# Libraries
LIBS = -lm -lopusfile -lglib-2.0 -lpthread $(PKG_LDFLAGS)
LIBS += -lstdc++

LDFLAGS = -logg -lz

ifeq ($(UNAME_S), Linux)
  CFLAGS += -fPIE -fstack-clash-protection
  CXXFLAGS += -fPIE -fstack-clash-protection
  LDFLAGS += -pie -Wl,-z,relro
  ifneq (,$(filter $(ARCH), x86_64 i386))
        CFLAGS += -fcf-protection
        CXXFLAGS += -fcf-protection
  endif
  ifneq ($(DEBUG), 1)
  LDFLAGS += -s
  endif
else ifeq ($(UNAME_S), Darwin)
  LIBS += -framework CoreAudio -framework CoreFoundation
  ifeq ($(USE_MACOS_MEDIA), 1)
    LIBS += -framework MediaPlayer -framework AppKit
  endif
endif

# Conditionally add  USE_DBUS is enabled
ifeq ($(USE_DBUS), 1)
  DEFINES += -DUSE_DBUS
endif

# Conditionally add macOS media integration
ifeq ($(USE_MACOS_MEDIA), 1)
  DEFINES += -DUSE_MACOS_MEDIA
endif

DEFINES += -DPREFIX=\"$(PREFIX)\"

# Conditionally add faad2 support if USE_FAAD is enabled
ifeq ($(USE_FAAD), 1)
  ifeq ($(ARCH), arm64)
    CFLAGS += -I/opt/homebrew/opt/faad2/include
    LIBS += -L/opt/homebrew/opt/faad2/lib -lfaad
  else ifeq ($(UNAME_O),Android)
    CFLAGS += -I$(PREFIX)/include
    LIBS += -L$(PREFIX)/lib -lfaad
  else
    CFLAGS += -I/usr/local/include
    LIBS += -L/usr/local/lib -lfaad
  endif
  DEFINES += -DUSE_FAAD
endif

ifeq ($(origin CC),default)
    CC := gcc
endif

ifneq ($(findstring gcc,$(CC)),)
    ifeq ($(UNAME_S), Linux)
        LIBS += -latomic
    endif
endif

OBJDIR = src/obj

SRCS = src/common/appstate.c src/ui/common_ui.c src/common/common.c \
       src/utils/utils.c src/utils/file.c src/utils/cache.c src/utils/term.c \
       src/sound/sound.c src/sound/m4a.c src/sound/sound_builtin.c src/sound/audiobuffer.c \
       src/sound/decoders.c src/sound/audio_file_info.c src/sound/playback.c src/sound/volume.c \
       src/sys/sys_integration.c src/sys/notifications.c src/sys/mpris.c src/sys/discord_rpc.c \
       src/ops/playback_ops.c src/ops/playback_clock.c src/ops/playback_system.c \
       src/ops/playlist_ops.c src/ops/library_ops.c src/ops/track_manager.c src/ops/playback_state.c \
       src/ui/control_ui.c  src/ui/input.c src/ui/playlist_ui.c  src/ui/search_ui.c  src/ui/player_ui.c \
       src/ui/visuals.c src/ui/chroma.c src/ui/queue_ui.c src/ui/settings.c  src/ui/cli.c \
       src/data/theme.c src/data/directorytree.c src/data/lyrics.c src/data/img_func.c src/data/song_loader.c  \
       src/data/playlist.c  src/kew.c

# TagLib wrapper
WRAPPER_SRC = src/data/tagLibWrapper.cpp
WRAPPER_OBJ = $(OBJDIR)/tagLibWrapper.o

# macOS Now Playing bridge (Objective-C)
ifeq ($(USE_MACOS_MEDIA), 1)
MACOS_MEDIA_SRC = src/sys/macos_nowplaying.m
MACOS_MEDIA_OBJ = $(OBJDIR)/sys/macos_nowplaying.o
endif

MAN_PAGE = kew.1
MAN_DIR ?= $(PREFIX)/share/man
DATADIR ?= $(PREFIX)/share
LOCALEDIR ?= $(DATADIR)/locale
THEMEDIR = $(DATADIR)/kew/themes
THEMESRCDIR := $(CURDIR)/themes

DEFINES += -DLOCALEDIR=\"$(LOCALEDIR)\"

all: kew

# Generate object lists
OBJS_C = $(SRCS:src/%.c=$(OBJDIR)/%.o)

NESTEGG_SRCS = include/nestegg/nestegg.c
NESTEGG_OBJS = $(NESTEGG_SRCS:include/nestegg/%.c=$(OBJDIR)/nestegg/%.o)

# All objects together
OBJS = $(OBJS_C) $(NESTEGG_OBJS)

# Create object directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

## Compile C sources
$(OBJDIR)/%.o: src/%.c Makefile | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

# Compile Objective-C sources in src/ (macOS only)
$(OBJDIR)/%.o: src/%.m Makefile | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFINES) -fobjc-arc -c -o $@ $<

# Compile explicit C++ sources in src/
$(OBJDIR)/%.o: src/%.cpp Makefile | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEFINES) -c -o $@ $<

# Compile TagLib wrapper C++ source
$(WRAPPER_OBJ): $(WRAPPER_SRC) Makefile | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEFINES) -c $< -o $@

# Compile C files in include/nestegg
$(OBJDIR)/nestegg/%.o: include/nestegg/%.c Makefile | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

# Collect optional objects
EXTRA_OBJS =
ifeq ($(USE_MACOS_MEDIA), 1)
  EXTRA_OBJS += $(MACOS_MEDIA_OBJ)
endif

# Link all objects safely together using C++ linker
kew: $(OBJS) $(WRAPPER_OBJ) $(EXTRA_OBJS) Makefile
	$(CXX) -o kew $(OBJS) $(WRAPPER_OBJ) $(EXTRA_OBJS) $(LIBS) $(LDFLAGS)

.PHONY: install
install: all
	# Create directories
	mkdir -p $(DESTDIR)$(MAN_DIR)/man1
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(THEMEDIR)
	mkdir -p $(DESTDIR)$(LOCALEDIR)/ja/LC_MESSAGES
	mkdir -p $(DESTDIR)$(LOCALEDIR)/zh_CN/LC_MESSAGES

	# Install binary and man page
	install -m 0755 kew $(DESTDIR)$(PREFIX)/bin/kew
	install -m 0644 docs/kew.1 $(DESTDIR)$(MAN_DIR)/man1/kew.1

	# Install Chinese translation
	install -m 0644 locale/zh_CN/LC_MESSAGES/kew.mo \
	$(DESTDIR)$(LOCALEDIR)/zh_CN/LC_MESSAGES/kew.mo

	# Install Japanese translation
	install -m 0644 locale/ja/LC_MESSAGES/kew.mo \
	$(DESTDIR)$(LOCALEDIR)/ja/LC_MESSAGES/kew.mo

	@if [ -d $(THEMESRCDIR) ]; then \
	for theme in $(THEMESRCDIR)/*.theme; do \
			if [ -f "$$theme" ]; then \
				install -m 0644 "$$theme" $(DESTDIR)$(THEMEDIR)/; \
			fi; \
		done; \
		for theme in $(THEMESRCDIR)/*.txt; do \
			if [ -f "$$theme" ]; then \
				install -m 0644 "$$theme" $(DESTDIR)$(THEMEDIR)/; \
			fi; \
		done; \
	fi

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/kew
	rm -f $(DESTDIR)$(MAN_DIR)/man1/kew.1
	rm -rf $(DESTDIR)$(THEMEDIR)
	rm -f $(DESTDIR)$(LOCALEDIR)/ja/LC_MESSAGES/kew.mo
	rm -f $(DESTDIR)$(LOCALEDIR)/zh_CN/LC_MESSAGES/kew.mo

.PHONY: clean
clean:
	rm -rf $(OBJDIR) kew
i18n:
	$(MAKE) -f Makefile.i18n i18n
