CC = gcc
PKG_CONFIG	?= pkg-config
CFLAGS = -I/usr/include/stb -Iinclude/imgtotxt/ext -Iinclude/imgtotxt -I/usr/include/ffmpeg -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/libavformat -Iinclude/miniaudio -O2 -g $(shell $(PKG_CONFIG) --cflags gio-2.0 chafa libavformat fftw3f opus opusfile vorbis)
CFLAGS += -fstack-protector-strong -Wformat -Werror=format-security -fPIE -fstack-protector -fstack-protector-strong -D_FORTIFY_SOURCE=2
CFLAGS += -Wall -Wpointer-arith
LIBS = -latomic -lpthread -lrt -pthread -lm -lfreeimage -lglib-2.0  $(shell $(PKG_CONFIG) --libs gio-2.0 chafa libavformat fftw3f opus opusfile vorbis vorbisfile)
LDFLAGS = -pie -Wl,-z,relro

OBJDIR = src/obj
PREFIX = /usr
SRCS = src/soundcommon.c src/player.c src/soundopus.c src/soundvorbis.c src/soundbuiltin.c src/soundpcm.c src/mpris.c src/playerops.c src/volume.c src/cutils.c src/soundgapless.c src/songloader.c src/file.c src/chafafunc.c src/cache.c src/metadata.c src/playlist.c src/stringfunc.c src/term.c  src/settings.c src/albumart.c src/visuals.c src/kew.c
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

MAN_PAGE = kew.1
MAN_DIR ?= $(PREFIX)/share/man

all: kew

$(OBJDIR)/%.o: src/%.c Makefile | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/write_ascii.o: include/imgtotxt/write_ascii.c Makefile | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

kew: $(OBJDIR)/write_ascii.o $(OBJS) Makefile
	$(CC) -o kew $(OBJDIR)/write_ascii.o $(OBJS) $(LIBS)

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(MAN_DIR)/man1
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 kew $(DESTDIR)$(PREFIX)/bin/kew
	install -m 0644 docs/kew.1 $(DESTDIR)$(MAN_DIR)/man1/kew.1

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/kew
	rm -f $(DESTDIR)$(MAN_DIR)/man1/kew.1

.PHONY: clean
clean:
	rm -rf $(OBJDIR) kew
