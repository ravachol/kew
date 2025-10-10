CC ?= gcc
CXX ?= g++
PKG_CONFIG ?= pkg-config

# --- Flags Kompilasi Eksplisit ---
# Menambahkan semua path include yang diperlukan secara manual untuk C dan C++
MANUAL_INCLUDES = -I/usr/include/opus -I/usr/include/chafa
LOCAL_INCLUDES = -Iinclude -Iinclude/miniaudio -Iinclude/nestegg -Iinclude/stb_image
PKG_CFLAGS = $(shell $(PKG_CONFIG) --cflags gio-2.0 chafa fftw3f opusfile vorbis ogg glib-2.0 taglib)

# Flags umum
BASE_FLAGS = -O2 -Wall -Wextra -fPIE -D_FORTIFY_SOURCE=2
DEFINES = -DUSE_DBUS -DPREFIX=\"/usr/local\"

# Menggabungkan semua flags
CFLAGS = $(BASE_FLAGS) $(MANUAL_INCLUDES) $(PKG_CFLAGS) $(LOCAL_INCLUDES) $(DEFINES)
CXXFLAGS = $(CFLAGS) -std=c++11

# --- Pustaka (Libraries) ---
LIBS = $(shell $(PKG_CONFIG) --libs gio-2.0 chafa fftw3f opus opusfile ogg vorbis vorbisfile glib-2.0 taglib) \
       -lstdc++ -lpthread -lm -lz -latomic
LDFLAGS = -pie

# --- Daftar Berkas Sumber ---
OBJDIR = src/obj

# Daftar semua berkas sumber .c dan .cpp dari direktori src
SRCS = src/common_ui.c src/common.c src/theme.c src/sound.c src/directorytree.c src/notifications.c \
       src/soundcommon.c src/m4a.c src/search_ui.c src/playlist_ui.c \
       src/player_ui.c src/soundbuiltin.c src/mpris.c src/playerops.c \
       src/utils.c src/file.c src/imgfunc.c src/cache.c src/songloader.c \
       src/playlist.c src/term.c src/settings.c src/visuals.c src/kew.c \
       src/tagLibWrapper.cpp src/lyrics.c

# --- PERUBAHAN DI SINI: Menambahkan nestegg.c secara eksplisit ---
NESTEGG_SRC = include/nestegg/nestegg.c
NESTEGG_OBJ = $(OBJDIR)/nestegg.o

# Membuat daftar file objek dari semua sumber
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)
OBJS := $(OBJS:src/%.cpp=$(OBJDIR)/%.o)
OBJS += $(NESTEGG_OBJ)


# --- Aturan Build ---
all: kew

kew: $(OBJS)
	@echo "==> Linking executable..."
	$(CXX) -o $@ $^ $(LIBS) $(LDFLAGS)
	@echo "==> Build complete: ./kew"

# Aturan untuk mengkompilasi berkas .c dari direktori src
$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(OBJDIR)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Aturan untuk mengkompilasi berkas .cpp dari direktori src
$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p $(OBJDIR)
	@echo "CXX $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- PERUBAHAN DI SINI: Aturan khusus untuk mengkompilasi nestegg.c ---
$(NESTEGG_OBJ): $(NESTEGG_SRC)
	@mkdir -p $(OBJDIR)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "==> Cleaning up..."
	rm -rf $(OBJDIR) kew

# --- Aturan Instalasi ---
.PHONY: install clean all

install: all
	@echo "==> Installing kew..."
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 kew $(DESTDIR)/usr/local/bin/
	@echo "==> Installation complete."