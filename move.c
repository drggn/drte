#include <stdlib.h>
#include <stdbool.h>

#include "display.h"
#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

static bool first_byte(unsigned char c);
static bool scroll_up(Buffer *b);
static bool scroll_down(Buffer *b);

// Returns true if c is the first byte of a character.
static bool
first_byte(unsigned char c) {
	if (c < 128 || (c & 0xC0) == 0xC0)
		return true;
	else
		return false;
}

// Scrolls up by one line.
static bool
scroll_up(Buffer *b) {
	size_t start = b->first_visible_char;

	while (gbf_at(b->gap_buffer, b->first_visible_char) != '\n') {
		if (b->first_visible_char == b->bytes) {
			b->first_visible_char = start;
			return false;
		}
		b->first_visible_char +=
			bytes(gbf_at(b->gap_buffer, b->first_visible_char));
	}
	b->first_visible_char +=
		bytes(gbf_at(b->gap_buffer, b->first_visible_char));
	b->redisplay = true;
	return true;
}

// Scrolls down by one line.
static bool
scroll_down(Buffer *b) {
	int nls = 0;

	if (b->first_visible_char == 0)
		return false;

	do {
		char c = gbf_at(b->gap_buffer, b->first_visible_char);
		b->first_visible_char -= bytes(c);
		if (gbf_at(b->gap_buffer, b->first_visible_char) == '\n') {
			nls++;
		}
	} while (nls != 2 && b->first_visible_char != 0);

	if (b->first_visible_char != 0)
		b->first_visible_char++;

	b->redisplay = true;
	return true;
}

// Move the line with the cursor to the center of the screen
void
uf_center(Editor *e) {
	Buffer *b = e->current;
	size_t start = b->first_visible_char;
	size_t goal = b->window.size.lines / 2;

	while (b->cursor.line < goal) {
		if (!scroll_down(b)) {
			message(e, "Can't scroll down here.");
			b->first_visible_char = start;
			return;
		}
		b->cursor.line++;
	}
	while (b->cursor.line > goal) {
		if (!scroll_up(b)) {
			message(e, "Can't scroll up here.");
			b->first_visible_char = start;
			return;
		}
		b->cursor.line--;
	}
}

// Moves the cursor one position to the left.
void
uf_left(Editor *e) {
	Buffer *b = e->current;

	if (b->offset == 0) {
		message(e, "Beginning of buffer");
		return;
	}
	do {
		b->offset--;
	} while (!first_byte(gbf_at(b->gap_buffer, b->offset)));

	if (b->cursor.column == 0) {
		b->line--;

		if (b->cursor.line == 0) {
			scroll_down(b);
			uf_bol(e);
			uf_eol(e);
		} else {
			b->cursor.line--;
			b->cursor.column = e->line_length[b->cursor.line];
		}
	} else {
		b->cursor.column -= width(gbf_at(b->gap_buffer, b->offset));
	}
}

// Moves the cursor one position to the right.
void
uf_right(Editor *e) {
	Buffer *b = e->current;

	if (b->offset == b->bytes) {
		message(e, "End of buffer");
		return;
	}
	if (gbf_at(b->gap_buffer, b->offset) == '\n') {
		if (b->cursor.line >= b->window.size.lines - 1) {
			scroll_up(b);
		} else {
			b->cursor.line++;
		}
		b->line++;
		b->cursor.column = 0;
	} else {
		b->cursor.column += width(gbf_at(b->gap_buffer, b->offset));
	}
	unsigned char c = gbf_at(b->gap_buffer, b->offset);
	b->offset += bytes(c);
}

// Moves the cursor one line up.
void
uf_up(Editor *e) {
	Buffer *b = e->current;
	static size_t p;

	if (b->lastfunc != uf_up)
		p = e->text_buffer->cursor.column;

	while (b->cursor.column != 0) {
		uf_left(e);
	}
	do {
		uf_left(e);
	} while (b->offset != 0 && b->cursor.column > p);
}

// Moves the cursor one line down.
void
uf_down(Editor *e) {
	Buffer *b = e->current;
	static size_t p;
	size_t w;

	if (b->lastfunc != uf_down)
		p = e->text_buffer->cursor.column;

	// Move the cursor to the beginning of the next line
	do {
		uf_right(e);
	} while (b->cursor.column > p && b->offset != b->bytes);

	while (gbf_at(b->gap_buffer, b->offset) != '\n' && b->offset != b->bytes) {
		w = width(gbf_at(b->gap_buffer, b->offset));
		if (b->cursor.column + w > p) {
			break;
		}
		uf_right(e);
	}
}

// Moves the cursor to the beginning of the line.
void
uf_bol(Editor *e) {
	Buffer *b = e->current;

	if (b->offset == 0)
		return;
	do {
		uf_left(e);
	} while (b->offset != 0 && gbf_at(b->gap_buffer, b->offset) != '\n');
	b->cursor.column = 0;
}

// Moves the cursor to the end of the line.
void
uf_eol(Editor *e) {
	Buffer *b = e->current;

	if (b->offset == b->bytes) {
		message(e, "End of buffer");
		return;
	}
	while (b->offset != b->bytes && gbf_at(b->gap_buffer, b->offset) != '\n') {
		uf_right(e);
	}
}

void
uf_page_down(Editor *e) {
	Buffer *b = e->current;
	size_t start = b->first_visible_char;

	for (size_t i = 0; i < b->window.size.lines - 1; i++) {
		if (!scroll_up(b)) {
			b->first_visible_char = start;
			message(e, "End of buffer");
			return;
		}
	}
	b->offset = b->first_visible_char;
	b->cursor.line = 0;
	b->cursor.column = 0;
}

void
uf_page_up(Editor *e) {
	Buffer *b = e->current;

	if (b->first_visible_char == 0) {
		message(e, "Beginning of buffer");
		return;
	}
	for (size_t i = 0; i < b->window.size.lines - 1; i++) {
		if (!scroll_down(b)) {
			b->first_visible_char = 0;
		}
	}
	b->offset = b->first_visible_char;
	b->cursor.line = 0;
	b->cursor.column = 0;
}
