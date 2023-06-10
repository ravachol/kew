CC = gcc
CFLAGS = -Iinclude/imgtotxt -Iinclude/miniaudio
LIBS = -lpthread -lavformat -lavutil -L/usr/lib  -lfftw3_omp -lfftw3 -lfftw3f_omp -lfftw3f -lm 

OBJDIR = src/obj

SRCS = src/sound.c src/dir.c src/printfunc.c src/settings.c src/playlist.c src/visuals.c src/events.c src/stringextensions.c src/file.c src/term.c src/metadata.c src/albumart.c src/cue.c
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