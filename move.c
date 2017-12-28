#include <stdlib.h>

#include <ncurses.h>

#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

// Returns true if c is the first byte of a character.
static int
fstbyte(unsigned char c){
	if(c < 128 || (c & 0xC0) == 0xC0)
		return 1;
	else
		return 0;
}

// Sets b->off to the beginning of the next character.
void
forwoff(Buffer *b){
	int i;
	unsigned char c = gbfat(b->gbuf, b->off);
	i = bytes(c);
	if((b->off + i) <= b->bytes)
		b->off += i;
}

// Sets b->off to the beginning of the previous character.
void
backoff(Buffer *b){
	if(b->off != 0){
		do{
			b->off--;
		}while(!fstbyte(gbfat(b->gbuf, b->off)));
	}
}

static void
forwvis(Buffer *b){
	size_t start = b->vis;

	while(gbfat(b->gbuf, b->vis) != '\n'){
		if(b->vis == b->bytes){
			b->vis = start;
			return;
		}
		b->vis++;
	}
	b->vis++;
}

static void
backwvis(Buffer *b){
	int nls = 0; // how many \n did we see?

	if(b->vis == 0)
		return;
	do{
		b->vis--;
		if(gbfat(b->gbuf, b->vis) == '\n')
			nls++;
	}while(b->vis > 0 && nls != 2);
	if(nls == 2)
		b->vis++;
}

static void
forwcol(Buffer *b){
	b->curcol += width(gbfat(b->gbuf, b->off));
}

static void
backwcol(Buffer *b){
	b->curcol -= width(gbfat(b->gbuf, b->off));
}

// Scrolls up by one line.
static void
scrollup(Buffer *b){
	forwvis(b);
	wclear(b->win);
	b->redisp = 1;
}

// Scrolls down by one line.
static void
scrolldown(Buffer *b){
	backwvis(b);
	wclear(b->win);
	b->redisp = 1;
}

// Moves the cursor one position to the left.
void
left(Editor *e){
	Buffer *b = e->current;

	if(b->off == 0){
		msg(e, "Beginning of buffer");
		return;
	}
	backoff(b);
	if(gbfat(b->gbuf, b->off) == '\n'){
		b->line--;
		if(b->curline == 0){
			scrolldown(b);
		}else{
			b->curline--;
		}
		bol(e);
		eol(e);
	}else{
		backwcol(b);
	}
}

// Moves the cursor one position to the right.
void
right(Editor *e){
	int x;
	int y;
	Buffer *b = e->current;

	if(b->off == b->bytes){
		msg(e, "End of buffer");
		return;
	}
	if(gbfat(b->gbuf, b->off) == '\n'){
		b->line++;
		getyx(b->win, y, x);
		(void)x; // suppresses warnings
		if(y == LINES - 2){
			scrollup(b);
		}else{
			b->curline++;
		}
		b->curcol = 0;
	}else{
		forwcol(b);
	}
	forwoff(b);
}

// Moves the cursor one line up.
void
up(Editor *e){
	Buffer *b = e->current;
	static size_t p;
	size_t w;

	if(b->lastfunc != up)
		p = e->txtbuf->curcol;

	bol(e);
	left(e);
	bol(e);

	while(b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n'){
		w = width(gbfat(b->gbuf, b->off));
		if(b->curcol + w <= p){
			forwoff(b);
			b->curcol += w;
		}else{
			break;
		}
	}
}

// Moves the cursor one line down.
void
down(Editor *e){
	Buffer *b = e->current;
	static size_t p;
	size_t w;

	if(b->lastfunc != down)
		p = e->txtbuf->curcol;
	eol(e);
	right(e);

	while(b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n'){
		w = width(gbfat(b->gbuf, b->off));
		if(b->curcol + w <= p){
			forwoff(b);
			b->curcol += w;
		}else{
			break;
		}
	}

}

// Moves the cursor to the beginning of the line.
void
bol(Editor *e){
	Buffer *b = e->current;

	if(b->off == 0)
		return;
	do{
		backoff(b);
		if(gbfat(b->gbuf, b->off) == '\n'){
			b->off++;
			break;
		}
	}while(b->off != 0);
	b->curcol = 0;
}

// Moves the cursor to the end of the line.
void
eol(Editor *e){
	Buffer *b = e->current;

	if(b->off == b->bytes){
		msg(e, "End of buffer");
		return;
	}
	while(b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n'){
		forwcol(b);
		forwoff(b);
	}
}
