#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

// Inserts s into the buffer.
void
ins(Editor *e, char *s) {
	Buffer *b = e->current;

	gbfins(b->gbuf, s, b->off);
	char *t = gbftxt(b->gbuf);
	free(t);
	b->bytes += strlen(s);
	right(e);
	b->changed = 1;
	b->redisp = 1;
}

// Inserts a '\t' at the current position
void
tab(Editor *e) {
	ins(e, "\t");
}

// Inserts a '\n' at the current position
void
newl(Editor *e) {
	ins(e, "\n");
}

// Deletes the char under the cursor
void
del(Editor *e) {
	Buffer *b = e->current;
	size_t bs = bytes(gbfat(b->gbuf, b->off));

	if (b->off == b->bytes) {
		msg(e, "End of buffer");
		return;
	}
	gbfdel(b->gbuf, b->off, bs);
	b->bytes -= bs;
	b->changed = 1;
	b->redisp = 1;
}

// Deletes the char before the cursor.
void
bksp(Editor *e) {
	if (e->current->off == 0) {
		msg(e, "Beginning of Buffer");
		return;
	}
	left(e);
	del(e);
}
