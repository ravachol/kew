CC = gcc -g
CFLAGS = -Iinclude/imgtotxt -Iinclude/miniaudio -O1
LIBS = -lm -lpthread -lavformat -lavutil

OBJDIR = src/obj

SRCS = src/events.c src/stringextensions.c src/file.c src/sound.c src/play.c include/imgtotxt/write_ascii.c
OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

all: play

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

play: $(OBJS)
	$(CC) -o play $(OBJS) $(LIBS)

.PHONY: install
install: all
	cp play /usr/local/bin/

.PHONY: clean
clean:
	rm -rf $(OBJDIR) play