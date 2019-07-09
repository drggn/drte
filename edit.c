#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "display.h"
#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

// Inserts s into the buffer.
void
insert(Editor *e, char *s) {
	Buffer *b = e->current;

	gbf_insert(b->gap_buffer, s, b->offset);
	b->bytes += strlen(s);
	uf_right(e);
	b->changed = true;
	b->redisplay = true;
}

// Inserts a '\t' at the current position
void
uf_tab(Editor *e) {
	insert(e, "\t");
}

// Inserts a '\n' at the current position
void
uf_newline(Editor *e) {
	insert(e, "\n");
}

// Deletes the char under the cursor
void
uf_delete(Editor *e) {
	Buffer *b = e->current;
	size_t bs = bytes(gbf_at(b->gap_buffer, b->offset));

	if (b->offset == b->bytes) {
		message(e, "End of buffer");
		return;
	}
	gbf_delete(b->gap_buffer, b->offset, bs);
	b->bytes -= bs;
	b->changed = true;
	b->redisplay = true;
}

// Deletes the char before the cursor.
void
uf_backspace(Editor *e) {
	if (e->current->offset == 0) {
		message(e, "Beginning of Buffer");
		return;
	}
	uf_left(e);
	uf_delete(e);
}
