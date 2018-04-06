#include <stdlib.h>

#include <ncurses.h>

#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

// Returns true if c is the first byte of a character.
static int
fstbyte(unsigned char c) {
	if (c < 128 || (c & 0xC0) == 0xC0)
		return 1;
	else
		return 0;
}

// Sets b->off to the beginning of the next character.
void
forwoff(Buffer *b) {
	int i;
	unsigned char c = gbfat(b->gbuf, b->off);
	i = bytes(c);
	if ((b->off + i) <= b->bytes)
		b->off += i;
}

// Sets b->off to the beginning of the previous character.
void
backoff(Buffer *b) {
	if (b->off != 0) {
		do {
			b->off--;
		} while (!fstbyte(gbfat(b->gbuf, b->off)));
	}
}

static int
forwstartvis(Buffer *b) {
	size_t start = b->startvis;

	while (gbfat(b->gbuf, b->startvis) != '\n') {
		if (b->startvis == b->bytes) {
			b->startvis = start;
			return 0;
		}
		b->startvis += bytes(gbfat(b->gbuf, b->startvis));
	}
	b->startvis += bytes(gbfat(b->gbuf, b->startvis));
	return 1;
}

static int
backwstartvis(Buffer *b) {
	int nls = 0;

	if (b->startvis == 0)
		return 0;

	do {
		char c = gbfat(b->gbuf, b->startvis);
		b->startvis -= bytes(c);
		if (gbfat(b->gbuf, b->startvis) == '\n') {
			nls++;
		}
	} while (nls != 2 && b->startvis != 0);

	if (b->startvis != 0)
		b->startvis++;

	return 1;
}

static void
forwcol(Buffer *b) {
       b->curcol += width(gbfat(b->gbuf, b->off));
}

static void
backwcol(Buffer *b) {
       b->curcol -= width(gbfat(b->gbuf, b->off));
}

// Scrolls up by one line.
static int
scrollup(Buffer *b) {
	if (forwstartvis(b)) {
		b->redisp = 1;
		return 1;
	}
	return 0;
}

// Scrolls down by one line.
static int
scrolldown(Buffer *b) {
	if (backwstartvis(b)) {
		b->redisp = 1;
		return 1;
	}
	return 0;
}

// Move the line with the cursor to the center of the screen
void
center(Editor *e) {
	Buffer *b = e->current;
	size_t start = b->startvis;
	size_t goal = LINES / 2;

	while (b->curline < goal) {
		if (!scrolldown(b)) {
			msg(e, "Can't scroll down here.");
			b->startvis = start;
			return;
		}
		b->curline++;
	}
	while (b->curline > goal) {
		if (!scrollup(b)) {
			msg(e, "Can't scroll up here.");
			b->startvis = start;
			return;
		}
		b->curline--;
	}
}

// Moves the cursor one position to the left.
void
left(Editor *e) {
	Buffer *b = e->current;

	if (b->off == 0) {
		msg(e, "Beginning of buffer");
		return;
	}
	backoff(b);

	if (b->curcol == 0) {
		b->line--;

		if (b->curline == 0) {
			scrolldown(b);
			bol(e);
			eol(e);
		} else {
			b->curline--;
			b->curcol = e->linelength[b->curline];
		}
	} else {
		backwcol(b);
	}
}

// Moves the cursor one position to the right.
void
right(Editor *e) {
	Buffer *b = e->current;

	if (b->off == b->bytes) {
		msg(e, "End of buffer");
		return;
	}
	if (gbfat(b->gbuf, b->off) == '\n') {
		if (b->curline >= LINES - 2) {
			scrollup(b);
		} else {
			b->curline++;
		}
		b->line++;
		b->curcol = 0;
	} else {
		forwcol(b);
	}
	forwoff(b);
}

// Moves the cursor one line up.
void
up(Editor *e) {
	Buffer *b = e->current;
	static size_t p;

	if (b->lastfunc != up)
		p = e->txtbuf->curcol;

	while (b->curcol != 0) {
		left(e);
	}
	do {
		left(e);
	} while (b->off != 0 && b->curcol > p);
}

// Moves the cursor one line down.
void
down(Editor *e) {
	Buffer *b = e->current;
	static size_t p;
	size_t w;

	if (b->lastfunc != down)
		p = e->txtbuf->curcol;

	// Move the cursor to the beginning of the next line
	do {
		right(e);
	} while (b->curcol > p && b->off != b->bytes);

	while (gbfat(b->gbuf, b->off) != '\n' && b->off != b->bytes) {
		w = width(gbfat(b->gbuf, b->off));
		if (b->curcol + w > p) {
			break;
		}
		right(e);
	}
}

// Moves the cursor to the beginning of the line.
void
bol(Editor *e) {
	Buffer *b = e->current;

	if (b->off == 0)
		return;
	do {
		backoff(b);
		if (gbfat(b->gbuf, b->off) == '\n') {
			b->off++;
			break;
		}
	} while (b->off != 0);
	b->curcol = 0;
}

// Moves the cursor to the end of the line.
void
eol(Editor *e) {
	Buffer *b = e->current;

	if (b->off == b->bytes) {
		msg(e, "End of buffer");
		return;
	}
	while (b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n') {
		right(e);
	}
}

void
pgdown(Editor *e) {
	Buffer *b = e->current;
	size_t start = b->startvis;

	for (size_t i = 0; i < LINES - 1; i++) {
		if (!scrollup(b)) {
			b->startvis = start;
			msg(e, "End of buffer");
			return;
		}
	}
	b->off = b->startvis;
	b->curline = 0;
	b->curcol = 0;
}

void
pgup(Editor *e) {
	Buffer *b = e->current;
	size_t start = b->startvis;

	for (size_t i = 0; i < LINES - 1; i++) {
		if (!scrolldown(b)) {
			b->startvis = start;
			msg(e, "Beginning of buffer");
			return;
		}
	}
	b->off = b->startvis;
	b->curline = 0;
	b->curcol = 0;
}
