CC = gcc
PKG_CONFIG	?= pkg-config
CFLAGS += -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -pedantic -g -Iinclude/imgtotxt $(shell $(PKG_CONFIG) --cflags gio-2.0 stb chafa libavformat fftw3f)
LIBS = -latomic -lpthread -lrt -pthread -lm -lfreeimage -lglib-2.0 $(shell $(PKG_CONFIG) --libs gio-2.0 chafa libavformat fftw3f)

OBJDIR = src/obj
PREFIX = /usr
SRCS = src/mpris.c src/playerops.c src/volume.c src/cutils.c src/soundgapless.c src/songloader.c src/file.c src/chafafunc.c src/cache.c src/metadata.c src/playlist.c src/stringfunc.c src/term.c  src/settings.c src/player.c src/albumart.c src/visuals.c src/kew.c
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

MAN_PAGE = kew.1 
MAN_DIR ?= $(PREFIX)/share/man

all: kew

$(OBJDIR)/%.o: src/%.c Makefile | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/write_ascii.o: include/imgtotxt/write_ascii.c Makefile | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

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
