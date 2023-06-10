CC = gcc
CFLAGS = -Iinclude/imgtotxt -Iinclude/miniaudio -O1
LIBS = -lpthread -lavformat -lavutil -L/usr/lib  -lfftw3_omp -lfftw3 -lfftw3f_omp -lfftw3f -lm 

OBJDIR = src/obj

SRCS =  src/sound.c src/events.c src/stringextensions.c src/file.c src/term.c src/cue.c include/imgtotxt/write_ascii.c
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

all: cue

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

cue: $(OBJS)
	$(CC) -o cue $(OBJS) $(LIBS)

.PHONY: install
install: all
	cp cue /usr/local/bin/

.PHONY: clean
clean:
	rm -rf $(OBJDIR) cue