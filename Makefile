CC = gcc
CFLAGS = -Os -Wall -std=c99 -pedantic -D_DEFAULT_SOURCE
BIN = drte

#############################################################################

OBJS = main.o gapbuf.o utils.o edit.o move.o mgmt.o display.o

drte: $(OBJS)
	$(CC) $(CFLAGS) -o $@ *.o

drte-static: $(OBJS)
	$(CC) $(CFLAGS) -static -o $@ *.o

clean:
	-rm *.o

distclean:
	-rm *.o drte drte-static

main.o: main.c gapbuf.h utils.h buf.h funcs.h display.h
gapbuf.o: gapbuf.c gapbuf.h utils.h
utils.o: utils.c utils.h
edit.o: edit.c gapbuf.h buf.h utils.h funcs.h display.h
move.o: move.c gapbuf.h buf.h utils.h funcs.h display.h
mgmt.o: mgmt.c display.h gapbuf.h buf.h utils.h funcs.h
display.o: display.c display.h gapbuf.h buf.h
