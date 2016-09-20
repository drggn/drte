#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ncurses.h>

#include "xmalloc.h"
#include "gapbuf.h"
#include "buf.h"
#include "funcs.h"

#define width(c) ((c) == '\t' ? 8 : 1)

#define streql(s, t) (strcmp((s), (t)) == 0)

// Returns true if c is the first byte of a character.
static int
fstbyte(unsigned char c){
	if(c < 128 || (c & 0xC0) == 0xC0)
		return 1;
	else
		return 0;
}

// Given the first byte of a code point, the function
// returns the number of bytes the code points consist of.
static size_t
bytes(unsigned char c){
	if(c < 128)
		return 1;
	else if((c & 0xF0) == 0xF0)
		return 4;
	else if((c & 0xE0) == 0xE0)
		return 3;
	else if((c & 0xC0) == 0xC0)
		return 2;
	return 0;
}

// Sets b->off to the beginning of the next character.
static void
forwoff(Buffer *b){
	int i = 0;
	unsigned char c = gbfat(b->gbuf, b->off);
	i = bytes(c);
	if((b->off + i) <= b->bytes)
		b->off += i;
}

// Sets b->off to the beginning of the previous character.
static void
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

// Displays prompt in the status bar. Returns
// the text entered by the user.
static char *
prompt(Editor *e, char *prompt){
	char *txt;
	size_t coloff = strlen(prompt);
	gbfclear(e->prbuf->gbuf);
	e->prbuf->col = 0;
	e->prbuf->off = 0;
	e->prbuf->bytes = 0;
	txt = gbftxt(e->prbuf->gbuf);

 loop:
	wclear(e->bar);
	free(txt);
	txt = gbftxt(e->prbuf->gbuf);
	mvwaddstr(e->bar, 0, 0, prompt);
	mvwaddstr(e->bar, 0, coloff, txt);

	wrefresh(e->bar);

	int c = wgetch(e->bar);
	if(c == '\n'){
		return gbftxt(e->prbuf->gbuf);
	}else if(c == 3 || c == 7){
		// C-c or C-g
		return NULL;
	}else if((c >= 0 && c <= 31) || (c >= KEY_BREAK && c <= KEY_UNDO)){
		if(e->prfuncs[Code(c)] != NULL){
			e->prfuncs[Code(c)](e);
		}
	}else{
		int b;
		char inp[5] = {0};
		if(c < 128){
			// one byte
			b = 1;
		}else if((c & 0x000000F0) == 0xF0){
			// for bytes
			b = 4;
		}else if((c & 0x000000E0) == 0xE0){
			// three bytes
			b = 3;
		}else if((c & 0x000000C0) == 0xC0){
			// two bytes
			b = 2;
		}else{
			flash();
			goto loop;
		}
		inp[0] = c;
		for(int i = 1; i < b; i++){
			c = wgetch(e->bar);
			if((c & 0x00000080) == 0x00000080){
				inp[i] = c;
			}else{
				flash();
				goto loop;
			}
		}
		ins(e->prbuf, inp);
		goto loop;
	}
	return NULL;
}

// Inserts s into the buffer.
void
ins(Buffer *buf, char *s){
	gbfins(buf->gbuf, s, buf->off);
	buf->bytes += strlen(s);
	forwoff(buf);
	buf->col += width(s[0]);
	buf->changed = 1;
	buf->redisp = 1;
}

// Inserts a '\t' at the current position
void
tab(Editor *e){
	ins(e->current, "\t");
}

// Inserts a '\n' at the current position
void
newl(Editor *e){
	ins(e->current, "\n");
	e->current->line++;
	e->current->curline++;
	e->current->col = 0;
}

// Deletes the char under the cursor
void
del(Editor *e){
	Buffer *b = e->current;
	size_t bs = bytes(gbfat(b->gbuf, b->off));

	if(b->off == b->bytes){
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
bksp(Editor *e){
	if(e->current->off == 0){
		msg(e, "Beginning of Buffer");
		return;
	}
	left(e);
	del(e);
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
		b->col -= width(gbfat(b->gbuf, b->off));
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
		b->col = 0;
	}else{
		b->col += width(gbfat(b->gbuf, b->off));
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
		p = e->current->col;

	bol(e);
	left(e);
	bol(e);

	while(b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n'){
		w = width(gbfat(b->gbuf, b->off));
		if(b->col + w <= p){
			forwoff(b);
			b->col += w;
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
		p = e->current->col;
	eol(e);
	right(e);

	while(b->off != b->bytes && gbfat(b->gbuf, b->off) != '\n'){
		w = width(gbfat(b->gbuf, b->off));
		if(b->col + w <= p){
			forwoff(b);
			b->col += w;
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
	b->col = 0;
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
		b->col += width(gbfat(b->gbuf, b->off));
		forwoff(b);
	}
}

// Adds buffer b to the editor. If before is true,
// b is inserted before the current buffer. Else after.
void
addbuffer(Editor *e, Buffer *buf, int before){
	if(buf == NULL)
		return;
	if(e->current == NULL){
		e->current = buf;
		buf->next = buf;
		buf->prev = buf;
	}else if(before){
		buf->next = e->current;
		buf->prev = e->current->prev;
		buf->prev->next = buf;
		e->current->prev = buf;
	}else{
		buf->next = e->current->next;
		e->current->next = buf;
		buf->prev = e->current;
		buf->next->prev = buf;
	}
}

// Creates a new Buffer with the contents of
// file. If file is NULL an empty Buffer is created.
Buffer *
newbuffer(Editor *e, char *file){
	struct stat st;
	off_t size;
	Buffer *buf;

	if(file == NULL){
		size = 0;
	}else if(stat(file, &st) != 0){
		if(errno != ENOENT){
			msg(e, "Can't stat file.");
			return NULL;
		}else{
			size = 0;
		}
	}else{
		size = st.st_size;
	}
	buf = xmalloc(sizeof(*buf));
	memset(buf, 0, sizeof(*buf));
	if((buf->win = newwin(LINES - 1, 0, 0, 0)) == NULL){
		msg(e, "Can't create a window.");
		free(buf);
		return NULL;
	}
	if(buf->win == NULL)
		return NULL;

	buf->bytes = size;
	buf->gbuf = gbfnew(file, size);
	if(file != NULL){
		buf->filename = xmalloc(strlen(file) + 1);
		strcpy(buf->filename, file);
	}
	keypad(buf->win, TRUE);

	return buf;
}

// Opens a file.
void
open(Editor *e){
	char *name = prompt(e, "Name: ");
	Buffer *b = e->current;
	if(name == NULL)
		return;
	// check for existng buffer
	do{
		if(streql(e->current->filename, name)){
			free(name);
			return;
		}
		b = b->next;
	}while(b != e->current);

	Buffer *buf = newbuffer(e, name);
	addbuffer(e, buf, 0);
	free(name);
	nextbuffer(e);
}

// Closes the current buffer.
void
close(Editor *e){
	Buffer *b = e->current;
	Buffer *new;
	char msg[1024];
	char *txt;

	if(b->changed){
		snprintf(msg, 1023, "%s has changed. Save? ",
				 e->current->filename);
		txt = prompt(e, msg);
		if(txt == NULL)
			return;
		if(streql(txt, "y") || streql(txt, "yes")){
			save(e);
		}
	}
	if(b->filename != NULL)
		free(b->filename);
	gbffree(b->gbuf);

	if(b->next == b){
		free(b);
		e->current = NULL;
		new = newbuffer(e, NULL);
		addbuffer(e, new, 0);
		nextbuffer(e);
	}else{
		b->next->prev = b->prev;
		b->prev->next = b->next;
		nextbuffer(e);
		free(b);
	}
}

// Saves a file.
// FIXME: always prints error, even if saved
void
save(Editor *e){
	FILE *fd;
	char *txt;
	char m[1024];
	Buffer *b = e->current;

	if(b->changed == 0){
		msg(e, "File hasn't changed");
		return;
	}
	if(b->filename == NULL){
		saveas(e);
		return;
	}

	txt = gbftxt(b->gbuf);

	if((fd = fopen(b->filename, "w")) == NULL){
		snprintf(m, 1023, "Can't open %s", b->filename);
		msg(e, m);
	}
	if(fwrite(txt, b->bytes, 1, fd) != 1){
		snprintf(m, 1023, "Error saving %s", b->filename);
		msg(e, m);
	}else{
		msg(e, "wrote file");
		e->current->changed = 0;
	}
	free(txt);
}

// Saves a file under a different name.
void
saveas(Editor *e){
	char* name = prompt(e, "Filename: ");
	if(name == NULL)
		return;
	e->current->filename = name;
	save(e);
}

// Quits drte.
void
quit(Editor *e){
	char *txt;
	char m[1024];
	Buffer *first = e->current;

	do{
		if(e->current->filename != NULL && e->current->changed == 1){
			snprintf(m, 1023, "Save %s? ", e->current->filename);
			txt = prompt(e, m);
			if(txt == NULL)
				return;
			if(streql(txt, "yes") || streql(txt, "y"))
				save(e);
			free(txt);
		}
		e->current = e->current->next;
	}while(e->current != first);

	exit(0);
}

// Selects the previous buffer in the buffer list.
void
prevbuffer(Editor *e){
	e->current = e->current->prev;
	e->current->redisp = 1;
	wclear(e->current->win);
}

// Advances to the next buffer in the buffer list.
void
nextbuffer(Editor *e){
	e->current = e->current->next;
	e->current->redisp = 1;
	wclear(e->current->win);
}

void
msg(Editor *e, char *msg){
	mvwaddstr(e->bar, 0, 0, msg);
	for(size_t i = strlen(msg); i <= COLS; i++)
		waddstr(e->bar, " ");
	e->msg = 1;
}

void
cx(Editor *e){
	int c = wgetch(e->current->win);
	switch(c){
	case Ctrl('S'):
	case 's': save(e); break;
	case Ctrl('W'):
	case 'w': saveas(e); break;
	case Ctrl('K'):
	case 'k': close(e); break;
	case Ctrl('C'):
	case 'c': quit(e); break;
	case Ctrl('N'):
	case 'n': nextbuffer(e); break;
	case Ctrl('P'):
	case 'p': prevbuffer(e); break;
	case Ctrl('F'):
	case 'f': open(e); break;
	default: msg(e, "Sequence not bound");
	}
}
