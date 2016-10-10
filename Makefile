CC ?= musl-gcc
CFLAGS ?= -Os -Wall -std=c99 -pedantic -g
NCURSES = -lncursesw
NCURSES-STATIC = -lncursesw -ltinfo
BIN = drte

#############################################################################

OBJS = main.o gapbuf.o xmalloc.o funcs.o

drte: $(OBJS)
	$(CC) $(CFLAGS) -o $@ *.o $(NCURSES)

drte-static: $(OBJS)
	$(CC) $(CFLAGS) -static -o $@ *.o $(NCURSES-STATIC)

clean:
	-rm *.o

distclean:
	-rm *.o drte drte-static

main.o: main.c gapbuf.h xmalloc.h buf.h funcs.h
gapbuf.o: gapbuf.c gapbuf.h xmalloc.h
xmalloc.o: xmalloc.c xmalloc.h
funcs.o: funcs.c funcs.h buf.h gapbuf.h xmalloc.h

