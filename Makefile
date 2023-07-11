CC = gcc
CFLAGS = -Iinclude/imgtotxt -Iinclude/miniaudio -I/usr/include/chafa -I/usr/lib/chafa/include -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -lO1 `pkg-config --cflags glib-2.0 chafa`
LIBS =  -lpthread -lavformat -lavutil -L/usr/lib -lfftw3_omp -lfftw3 -lfftw3f_omp -lfftw3f -lrt -pthread -lcurl -lm -lfreeimage -lchafa `pkg-config --libs glib-2.0 chafa`

OBJDIR = src/obj

SRCS = src/soundgapless.c src/songloader.c src/file.c src/chafafunc.c src/cache.c src/metadata.c src/printfunc.c src/playlist.c src/stringfunc.c src/term.c  src/settings.c src/player.c src/albumart.c src/visuals.c src/cue.c
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

all: cue

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/write_ascii.o: include/imgtotxt/write_ascii.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

cue: $(OBJDIR)/write_ascii.o $(OBJS)
	$(CC) -o cue $(OBJDIR)/write_ascii.o $(OBJS) $(LIBS)

.PHONY: install
install: all
	cp cue /usr/local/bin/

.PHONY: clean
clean:
	rm -rf $(OBJDIR) cue