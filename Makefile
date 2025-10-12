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

# Default USE_DBUS to auto-detect if not set by user
ifeq ($(origin USE_DBUS), undefined)
  ifeq ($(UNAME_S), Darwin)
    USE_DBUS = 0
  else
    USE_DBUS = 1
  endif
endif

# Adjust the PREFIX for macOS and Linux
ifeq ($(UNAME_S), Darwin)
    ifeq ($(ARCH), arm64)
        PREFIX ?= /usr/local
        PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/opt/homebrew/share/pkgconfig:$(PKG_CONFIG_PATH)
    else
        PREFIX ?= /usr/local
        PKG_CONFIG_PATH := /usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:$(PKG_CONFIG_PATH)
    endif
else
    PREFIX ?= /usr/local
    PKG_CONFIG_PATH := /usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig:$(PKG_CONFIG_PATH)
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
    USE_FAAD = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --exists faad && echo 1 || echo 0)

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

# Compiler flags
COMMONFLAGS = -Isrc -I/usr/include -I/opt/homebrew/include -I/usr/local/include -I/usr/lib -Iinclude/minimp4 \
         -I/usr/include/chafa -I/usr/lib/chafa/include -I/usr/lib64/chafa/include -I/usr/include/ogg -I/usr/include/opus \
         -I/usr/include/stb -Iinclude/stb_image -I/usr/include/glib-2.0 \
         -I/usr/lib/glib-2.0/include -I/usr/lib64/glib-2.0/include -Iinclude/miniaudio -Iinclude -Iinclude/nestegg -I/usr/include/gdk-pixbuf-2.0

ifeq ($(DEBUG), 1)
COMMONFLAGS += -g -DDEBUG
else
COMMONFLAGS += -O2
endif

COMMONFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --cflags gio-2.0 chafa fftw3f opus opusfile vorbis ogg glib-2.0 taglib)
COMMONFLAGS += -DMA_NO_AAUDIO
COMMONFLAGS += -fstack-protector-strong -Wformat -Werror=format-security -fPIE -D_FORTIFY_SOURCE=2
COMMONFLAGS += -Wall -Wextra -Wpointer-arith

  # Check if we're in Termux environment
ifneq ($(wildcard /data/data/com.termux/files/usr),)
  # Termux environment
  COMMONFLAGS += -D__ANDROID__
endif

CFLAGS = $(COMMONFLAGS)

# Compiler flags for C++ code
CXXFLAGS = $(COMMONFLAGS) -std=c++11

# Libraries
LIBS = -L/usr/lib -lm -lopusfile -lglib-2.0 -lpthread $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKG_CONFIG) --libs gio-2.0 chafa fftw3f opus opusfile ogg vorbis vorbisfile glib-2.0 taglib)
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
endif

# Conditionally add  USE_DBUS is enabled
ifeq ($(USE_DBUS), 1)
  DEFINES += -DUSE_DBUS
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

SRCS = src/common/appstate.c src/ui/common_ui.c src/ui/control_ui.c src/ops/playback_ops.c src/ops/input.c \
       src/common/common.c src/data/theme.c src/sound/sound.c src/sys/systemintegration.c \
       src/ops/library_ops.c src/data/directorytree.c src/data/lyrics.c src/ops/playback_state.c src/sys/notifications.c \
       src/ops/playlist_ops.c src/sound/soundcommon.c src/sound/m4a.c src/ui/search_ui.c src/ui/playlist_ui.c \
       src/ops/trackmanager.c src/ops/playback_clock.c src/ops/playback_system.c src/ui/player_ui.c src/sound/soundbuiltin.c \
       src/sys/mpris.c src/utils/utils.c src/utils/file.c src/data/imgfunc.c src/utils/cache.c src/data/songloader.c \
       src/data/playlist.c src/utils/term.c src/ops/settings.c src/ui/visuals.c src/kew.c

# TagLib wrapper
WRAPPER_SRC = src/data/tagLibWrapper.cpp
WRAPPER_OBJ = $(OBJDIR)/tagLibWrapper.o

MAN_PAGE = kew.1
MAN_DIR ?= $(PREFIX)/share/man
DATADIR ?= $(PREFIX)/share
THEMEDIR = $(DATADIR)/kew/themes

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

# Link all objects safely together using C++ linker
kew: $(OBJS) $(WRAPPER_OBJ) Makefile
	$(CXX) -o kew $(OBJS) $(WRAPPER_OBJ) $(LIBS) $(LDFLAGS)

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(MAN_DIR)/man1
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(THEMEDIR)
	install -m 0755 kew $(DESTDIR)$(PREFIX)/bin/kew
	install -m 0644 docs/kew.1 $(DESTDIR)$(MAN_DIR)/man1/kew.1
	@if [ -d themes ]; then \
		for theme in themes/*.theme; do \
			if [ -f "$$theme" ]; then \
				install -m 0644 "$$theme" $(DESTDIR)$(THEMEDIR)/; \
			fi; \
		done; \
		for theme in themes/*.txt; do \
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

.PHONY: clean
clean:
	rm -rf $(OBJDIR) kew
