#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include <ncurses.h>

#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

static int
streql(char *s, char *t) {
	return strcmp(s, t) == 0;
}


// TODO: refactor
void
loop(Editor *e) {
	char *txt = gbftxt(e->txtbuf->gbuf);
	char *prmtxt = gbftxt(e->prbuf->gbuf);

	do {
		if (e->msg) {
			e->msg = 0;
		} else if (e->prompt) {
			size_t coloff = strlen(e->promptstr);
			if (e->prbuf->curcol == 0)
				e->prbuf->curcol = coloff;
			wclear(e->prbuf->win);
			free(prmtxt);

			prmtxt = gbftxt(e->prbuf->gbuf);
			mvwaddstr(e->prbuf->win, 0, 0, e->promptstr);
			mvwaddstr(e->prbuf->win, 0, coloff, prmtxt);
			wmove(e->prbuf->win, 0, e->prbuf->curcol + coloff);
			e->current = e->prbuf;
		} else {
			char msg[COLS + 1];
			snprintf(msg, COLS, "(%ld,%ld) startvis: %ld %ld/%ld: %s%s",
					 e->txtbuf->curline, e->txtbuf->curcol, e->txtbuf->startvis,
					 e->txtbuf->off, e->txtbuf->bytes,
					 e->txtbuf->filename ? e->txtbuf->filename : "Unnamed",
					 e->txtbuf->changed ? "*" : " ");

			mvwaddstr(e->prbuf->win, 0, 0, msg);
			for (size_t i = strlen(msg); i <= COLS; i++)
				waddstr(e->prbuf->win, " ");
			e->current = e->txtbuf;
		}
		wrefresh(e->prbuf->win);

		// Draw the text buffer.
		if (e->txtbuf->redisp) {
			int maxlines;
			int maxcols;
			size_t line = 0;
			size_t col = 0;
			int lastwhitecol = -1;
			int lastwhiteline = -1;

			free(txt);
			wclear(e->txtbuf->win);

			txt = gbftxt(e->txtbuf->gbuf);
			size_t current = e->txtbuf->startvis;
			getmaxyx(e->txtbuf->win, maxlines, maxcols);

			while (txt[current]) {
				size_t wid;
				char cp[5] = {0};
				int size = bytes(txt[current]);

				for (int i = 0; i < size; i++, current++) {
					cp[i] = txt[current];
				}
				wid = width(cp[0]);

				mvwaddstr(e->txtbuf->win, line, col, cp);
				col += wid;

				if (iswhitespace(cp[0])) {
					if (lastwhitecol == -1) {
						lastwhitecol = col;
						lastwhiteline = line;
					}
				} else {
					lastwhitecol = -1;
					lastwhiteline = -1;
				}
				if (cp[0] == '\n') {
					if (lastwhitecol != -1) {
						// draw trailing whitespace
						int c = lastwhitecol;
						int l = lastwhiteline;
						wattron(e->txtbuf->win, A_REVERSE);
						while ((c != col) || (l != line)) {
							mvwaddstr(e->txtbuf->win, l, c, ".");
							if (LineWrap(c)) {
								c = 0;
								l++;
							} else {
								c++;
							}
						}
						wattroff(e->txtbuf->win, A_REVERSE);
					}
					line++;
					col = 0;
					lastwhitecol = -1;
					lastwhiteline = -1;
					continue;
				} else if (LineWrap(col)) {
					line++;
					col = 0;
				}
				if (line >= maxlines)
					break;
			}
			wrefresh(e->txtbuf->win);
			e->txtbuf->redisp = 0;
		}
		wmove(e->current->win, e->current->curline, e->current->curcol);

		int c = wgetch(e->current->win);
		if ((c >= 0 && c <= 31) || (c >= KEY_MIN && c <= KEY_MAX)) {
			if (e->current->funcs[Code(c)] != NULL) {
				e->current->funcs[Code(c)](e);
				e->current->lastfunc = e->current->funcs[Code(c)];
			}
		} else {
			int b = bytes(c);
			char inp[5] = {0};

			inp[0] = c;
			for (int i = 1; i < b; i++) {
				c = wgetch(e->current->win);
				if ((c & 0x00000080) == 0x00000080) {
					inp[i] = c;
				} else {
					msg(e, "Invalid input");
					continue;
				}
			}
			ins(e, inp);
		}
	} while (!e->stop);
}

// Redraw the display
void
redraw(Editor *e) {
	e->current->redisp = 1;
}

// Called when the terminal is resized
static void
resize(Editor *e) {
	Buffer *b = e->current;
	wresize(b->win, LINES - 1, COLS);
	mvwin(e->prbuf->win, LINES - 1, 0);
	redraw(e);

	// prevent cursor from getting out of sync
	size_t start = b->startvis;
	b->curline = 0;
	b->curcol = 0;

	while (start != b->off) {
		char c = gbfat(b->gbuf, start);

		start += bytes(c);
		b->curcol += width(c);

		if (isnewline(c)) {
			b->curcol = 0;
			b->curline++;
		}
		if (LineWrap(b->curcol)) {
			b->curcol = 0;
			b->curline++;
		}
	}
	if (b->curline >= LINES) {
		center(e);
	}
}

// Displays prompt in the status bar. Returns
// the text entered by the user.
static char *
prompt(Editor *e, char *prompt) {
	e->prompt = 1;
	e->promptstr = prompt;
	e->prbuf->curcol = 0;
	e->prbuf->off = 0;
	e->prbuf->bytes = 0;
	gbfclear(e->prbuf->gbuf);

	loop(e);

	e->prompt = 0;
	e->stop = 0;
	if (e->cancel) {
		e->cancel = 0;
		return NULL;
	} else {
		return gbftxt(e->prbuf->gbuf);
	}
}

// Adds buffer b to the editor. If before is true,
// b is inserted before the current buffer. Else after.
void
addbuffer(Editor *e, Buffer *buf, int before) {
	if (buf == NULL)
		return;
	if (e->txtbuf == NULL) {
		e->txtbuf = buf;
		buf->next = buf;
		buf->prev = buf;
	} else if (before) {
		buf->next = e->txtbuf;
		buf->prev = e->txtbuf->prev;
		buf->prev->next = buf;
		e->txtbuf->prev = buf;
	} else {
		buf->next = e->txtbuf->next;
		e->txtbuf->next = buf;
		buf->prev = e->txtbuf;
		buf->next->prev = buf;
	}
}

// Creates a new Buffer with the contents of
// file. If file is NULL an empty Buffer is created.
Buffer *
newbuffer(Editor *e, char *file) {
	struct stat st;
	off_t size;
	Buffer *buf;

	if (file == NULL) {
		size = 0;
	} else if (stat(file, &st) != 0) {
		if (errno != ENOENT) {
			msg(e, "Can't stat file.");
			return NULL;
		} else {
			size = 0;
		}
	} else {
		size = st.st_size;
	}
	buf = xmalloc(sizeof(*buf));
	memset(buf, 0, sizeof(*buf));
	if ((buf->win = newwin(LINES - 1, 0, 0, 0)) == NULL) {
		msg(e, "Can't create a window.");
		free(buf);
		return NULL;
	}
	if (buf->win == NULL)
		return NULL;

	buf->bytes = size;
	buf->gbuf = gbfnew(file, size);
	if (file != NULL) {
		buf->filename = xmalloc(strlen(file) + 1);
		strcpy(buf->filename, file);
	}
	buf->redisp = 1;
	keypad(buf->win, TRUE);

	memset(buf->funcs, 0, (NCURSESKEYS * 8));

	buf->funcs[Ctrl('A')] = bol;
	buf->funcs[Ctrl('B')] = left;
	buf->funcs[Ctrl('D')] = del;
	buf->funcs[Ctrl('E')] = eol;
	buf->funcs[Ctrl('F')] = right;
	buf->funcs[Ctrl('H')] = bksp;
	buf->funcs[Ctrl('I')] = tab;
	buf->funcs[Ctrl('J')] = newl;
	buf->funcs[Ctrl('L')] = center;
	buf->funcs[Ctrl('M')] = newl;
	buf->funcs[Ctrl('N')] = down;
	buf->funcs[Ctrl('P')] = up;
	buf->funcs[Ctrl('V')] = pgdown;
	buf->funcs[Ctrl('X')] = cx;
	buf->funcs[Ctrl('Z')] = suspend;

	buf->funcs[Ncur(KEY_RESIZE)] = resize;

	buf->funcs[Ncur(KEY_UP)] = up;
	buf->funcs[Ncur(KEY_DOWN)] = down;
	buf->funcs[Ncur(KEY_LEFT)] = left;
	buf->funcs[Ncur(KEY_RIGHT)] = right;
	buf->funcs[Ncur(KEY_HOME)] = bol;
	buf->funcs[Ncur(KEY_END)] = eol;
	buf->funcs[Ncur(KEY_NPAGE)] = pgdown;
	buf->funcs[Ncur(KEY_DC)] = del;
	buf->funcs[Ncur(KEY_ENTER)] = newl;
	buf->funcs[Ncur(KEY_BACKSPACE)] = bksp;
	buf->funcs[Ncur(KEY_STAB)] = tab;
	buf->funcs[Ncur(KEY_F(2))] = save;
	buf->funcs[Ncur(KEY_F(3))] = open;
	buf->funcs[Ncur(KEY_F(4))] = prevbuffer;
	buf->funcs[Ncur(KEY_F(5))] = nextbuffer;
	buf->funcs[Ncur(KEY_F(8))] = close;
	buf->funcs[Ncur(KEY_F(10))] = quit;

	return buf;
}

// Opens a file.
void
open(Editor *e) {
	char *name = prompt(e, "Name: ");
	Buffer *b = e->txtbuf;
	if (name == NULL)
		return;
	// check for existng buffer
	do {
		if (b->filename != NULL && streql(b->filename, name)) {
			e->txtbuf = b;
			free(name);
			e->txtbuf->redisp = 1;
			return;
		}
		b = b->next;
	} while (b != e->txtbuf);

	Buffer *buf = newbuffer(e, name);
	addbuffer(e, buf, 0);
	free(name);
	nextbuffer(e);
}

// Closes the current buffer.
void
close(Editor *e) {
	Buffer *b = e->txtbuf;
	Buffer *new;
	char msg[1024];
	char *txt;

	if (b->changed) {
		snprintf(msg, 1023, "%s has changed. Save? ",
				 e->txtbuf->filename);
		txt = prompt(e, msg);
		if (txt == NULL)
			return;
		if (streql(txt, "y") || streql(txt, "yes")) {
			save(e);
		}
	}
	if (b->filename != NULL)
		free(b->filename);
	gbffree(b->gbuf);

	if (b->next == b) {
		free(b);
		e->txtbuf = NULL;
		new = newbuffer(e, NULL);
		addbuffer(e, new, 0);
		nextbuffer(e);
	} else {
		b->next->prev = b->prev;
		b->prev->next = b->next;
		nextbuffer(e);
		free(b);
	}
}

// Saves a file.
void
save(Editor *e) {
	FILE *fd;
	char *txt;
	char m[1024];
	Buffer *b = e->txtbuf;

	if (b->changed == 0) {
		msg(e, "File hasn't changed");
		return;
	}
	if (b->filename == NULL) {
		saveas(e);
		return;
	}

	txt = gbftxt(b->gbuf);

	if ((fd = fopen(b->filename, "w")) == NULL) {
		snprintf(m, 1023, "Can't open %s", b->filename);
		msg(e, m);
	}
	if (fwrite(txt, b->bytes, 1, fd) != 1) {
		snprintf(m, 1023, "Error saving %s", b->filename);
		msg(e, m);
	} else {
		msg(e, "wrote file");
		e->txtbuf->changed = 0;
	}
	free(txt);
}

// Saves a file under a different name.
void
saveas(Editor *e) {
	char* name = prompt(e, "Filename: ");
	if (name == NULL)
		return;
	e->txtbuf->filename = name;
	save(e);
}

void
stoploop(Editor *e) {
	e->stop = 1;
}

void
cancelloop(Editor *e) {
	e->stop = 1;
	e->cancel = 1;
}

// Quits drte.
void
quit(Editor *e) {
	char *txt;
	char m[1024];
	Buffer *first = e->txtbuf;

	do {
		if (e->txtbuf->filename != NULL && e->txtbuf->changed == 1) {
			snprintf(m, 1023, "Save %s? ", e->txtbuf->filename);
			txt = prompt(e, m);
			if (txt == NULL)
				return;
			if (streql(txt, "yes") || streql(txt, "y"))
				save(e);
			free(txt);
		}
		e->txtbuf = e->txtbuf->next;
	} while (e->txtbuf != first);

	e->stop = 1;
}

void
suspend(Editor *e) {
	kill(0, SIGTSTP);
}

// Selects the previous buffer in the buffer list.
void
prevbuffer(Editor *e) {
	e->txtbuf = e->txtbuf->prev;
	e->txtbuf->redisp = 1;
	wclear(e->txtbuf->win);
}

// Advances to the next buffer in the buffer list.
void
nextbuffer(Editor *e) {
	e->txtbuf = e->txtbuf->next;
	e->txtbuf->redisp = 1;
	wclear(e->txtbuf->win);
}

void
msg(Editor *e, char *msg) {
	// We can't show a msg when there is a prompt.
	if (e->txtbuf == e->prbuf) {
		flash();
		return;
	}
	mvwaddstr(e->prbuf->win, 0, 0, msg);
	for (size_t i = strlen(msg); i <= COLS; i++)
		waddstr(e->prbuf->win, " ");
	e->msg = 1;
}

void
cx(Editor *e) {
	int c = wgetch(e->txtbuf->win);
	switch(c) {
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
