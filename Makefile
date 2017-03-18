CC ?= musl-gcc
CFLAGS ?= -Os -Wall -std=c99 -pedantic -g
NCURSES = -lncursesw
NCURSES-STATIC = -lncursesw -ltinfo
BIN = drte

#############################################################################

OBJS = main.o gapbuf.o utils.o edit.o move.o mgmt.o

drte: $(OBJS)
	$(CC) $(CFLAGS) -o $@ *.o $(NCURSES)

drte-static: $(OBJS)
	$(CC) $(CFLAGS) -static -o $@ *.o $(NCURSES-STATIC)

clean:
	-rm *.o

distclean:
	-rm *.o drte drte-static

main.o: main.c gapbuf.h utils.h buf.h funcs.h
gapbuf.o: gapbuf.c gapbuf.h utils.h
utils.o: utils.c utils.h
edit.o: edit.c gapbuf.h buf.h utils.h funcs.h
move.o: move.c gapbuf.h buf.h utils.h funcs.h
mgmt.o: mgmt.c gapbuf.h buf.h utils.h funcs.h

