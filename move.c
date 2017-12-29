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

static int
forwvis(Buffer *b){
	size_t start = b->vis;

	while(gbfat(b->gbuf, b->vis) != '\n'){
		if(b->vis == b->bytes){
			b->vis = start;
			return 0;
		}
		b->vis++;
	}
	b->vis++;
	return -1;
}

static int
backwvis(Buffer *b){
	int nls = 0; // how many \n did we see?

	if(b->vis == 0)
		return 0;
	do{
		b->vis--;
		if(gbfat(b->gbuf, b->vis) == '\n')
			nls++;
	}while(b->vis > 0 && nls != 2);
	if(nls == 2)
		b->vis++;
	return 1;
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
static int
scrollup(Buffer *b){
	if(forwvis(b)){
		wclear(b->win);
		b->redisp = 1;
		return 1;
	}
	return 0;
}

// Scrolls down by one line.
static int
scrolldown(Buffer *b){
	if(backwvis(b)){
		wclear(b->win);
		b->redisp = 1;
		return 1;
	}
	return 0;
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
	int maxx;
	int maxy;
	Buffer *b = e->current;

	if(b->off == b->bytes){
		msg(e, "End of buffer");
		return;
	}

	getmaxyx(b->win, maxy, maxx);

	if(gbfat(b->gbuf, b->off) == '\n'){
		b->line++;
		b->curline++;
		b->curcol = 0;
	}else{
		forwcol(b);
		if(b->curcol >= maxx - 1){
			b->curcol = 0;
			b->curline++;
		}
	}
	if(b->curline >= maxy){
		scrollup(b);
		b->curline = maxy - 1;
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
