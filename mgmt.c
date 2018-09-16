#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#include "gapbuf.h"
#include "display.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

static int
streql(char *s, char *t) {
	return strcmp(s, t) == 0;
}

// Move the screen left/right.
static void
scrollhorizontal(Editor *e) {

	if ((e->txtbuf->curcol < (e->txtbuf->win.columns / 2))) {
		e->txtbuf->viscol = 0;
	} else {
		e->txtbuf->viscol = e->txtbuf->curcol - (e->txtbuf->win.columns / 2);
	}
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

			clear_window(e->prbuf->win);

			free(prmtxt);
			prmtxt = gbftxt(e->prbuf->gbuf);

			set_foreground(Yellow);
			display(e->prbuf->win, 0, 0, e->promptstr);
			reset_colors();
			display(e->prbuf->win, 0, coloff, prmtxt);
		} else {
			clear_window(e->prbuf->win);
		}

		// draw statusbar
		char status[e->prbuf->win.columns + 1];
		snprintf(status, e->prbuf->win.columns, "(%ld,%ld) startvis: %ld %ld/%ld: %s%s",
				 e->txtbuf->curline, e->txtbuf->curcol, e->txtbuf->startvis,
				 e->txtbuf->off, e->txtbuf->bytes,
				 e->txtbuf->filename ? e->txtbuf->filename : "Unnamed",
				 e->txtbuf->changed ? "*" : " ");

		set_background(Blue);
		display(e->statusbar, 0, 0, status);
		for (size_t i = strlen(status); i <= e->prbuf->win.columns; i++)
			display(e->statusbar, 0, 0 + i, " ");
		reset_colors();

		if (e->txtbuf->curcol >= e->txtbuf->viscol + e->txtbuf->win.columns
			|| e->txtbuf->curcol < e->txtbuf->viscol) {
			redraw(e);
		}
		// Draw the text buffer.
		if (e->txtbuf->redisp) {
			size_t line = 0;
			size_t col = 0;
			int lastwhitecol = -1;

			free(txt);
			clear_window(e->txtbuf->win);

			txt = gbftxt(e->txtbuf->gbuf);
			size_t current = e->txtbuf->startvis;

			if (e->txtbuf->curcol >= e->txtbuf->viscol + e->txtbuf->win.columns
				|| e->txtbuf->curcol < e->txtbuf->viscol) {
				scrollhorizontal(e);
			}

			while (txt[current]) {
				size_t wid;
				char cp[5] = {0};
				int size = bytes(txt[current]);

				for (int i = 0; i < size; i++, current++) {
					cp[i] = txt[current];
				}
				wid = width(cp[0]);

				if (e->txtbuf->viscol <= col &&
					col < (e->txtbuf->viscol + e->current->win.columns)) {
					display(e->txtbuf->win, line, col - e->txtbuf->viscol, cp);
				}
				if (iswhitespace(cp[0])) {
					if (lastwhitecol == -1) {
						lastwhitecol = col;
					}
				} else {
					lastwhitecol = -1;
				}

				if ((cp[0] == '\n') || (txt[current] == '\0')) {
					e->linelength[line] = col;

					if (lastwhitecol != -1) {
						// draw trailing whitespace
						int c = lastwhitecol;
						if ((txt[current] == '\0') && (txt[current - 1] != '\n'))
							col += wid;
						move_cursor(e->txtbuf->win, line, c);
						set_background(Red);
						while (c != col) {
							display(e->txtbuf->win, line, c, " ");
							c++;
						}
						reset_colors();
					}
					if (cp[0] == '\n') {
						line++;
						col = 0;
						lastwhitecol = -1;
					}
				} else  {
					col += wid;
				}
				if (line >= e->current->win.lines)
					break;
			}
			e->txtbuf->redisp = 0;
		}
		move_cursor(e->current->win, e->current->curline, e->current->curcol - e->current->viscol);
		refresh();

		int c = getchar();

		if (c == 127)
			c = Ctrl('H');
		if (c >= 0 && c <= 31) {
			Func f = e->current->funcs.ctrl[c];
			if (f != NULL) {
				f(e);
				e->current->lastfunc = f;
			}
		} else {
			int b = bytes(c);
			char inp[5] = {0};

			inp[0] = c;
			for (int i = 1; i < b; i++) {
				c = getchar();
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
void
resize(Editor *e) {
	Buffer *b = e->current;
	Buffer *buf = e->current;
	size_t lines;

	set_display_size(e);
	lines = e->display.lines;

	do {
		resize_window(&buf->win, lines - 2, e->display.columns);
		buf = buf->next;
	} while (buf != b);

	resize_window(&e->prbuf->win, 1, e->display.columns);
	move_window(&e->prbuf->win, lines, 1);
	move_window(&e->statusbar, lines - 1, 1);

	if (lines > e->maxlines) {
		if (realloc(e->linelength, lines * 2)) {
			e->maxlines = lines * 2;
		} else {
			fprintf(stderr, "Out of memory");
			exit(-1);
		}
	}
	if (b->curline >= lines) {
		center(e);
	}
	scrollhorizontal(e);

	// TODO: screen is not redrawn automatically, because main loop blocks
	redraw(e);
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
	e->current = e->prbuf;
	gbfclear(e->prbuf->gbuf);


	loop(e);

	e->prompt = 0;
	e->stop = 0;
	e->current = e->txtbuf;
	clear_window(e->prbuf->win);

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
	resize_window(&buf->win, e->display.lines - 2, e->display.columns);
	move_window(&buf->win, 1, 1);

	buf->bytes = size;
	buf->gbuf = gbfnew(file, size);
	if (file != NULL) {
		buf->filename = xmalloc(strlen(file) + 1);
		strcpy(buf->filename, file);
	}
	buf->redisp = 1;

	buf->funcs.ctrl[Ctrl('A')] = bol;
	buf->funcs.ctrl[Ctrl('B')] = left;
	buf->funcs.ctrl[Ctrl('D')] = del;
	buf->funcs.ctrl[Ctrl('E')] = eol;
	buf->funcs.ctrl[Ctrl('F')] = right;
	buf->funcs.ctrl[Ctrl('H')] = bksp;
	buf->funcs.ctrl[Ctrl('I')] = tab;
	buf->funcs.ctrl[Ctrl('J')] = newl;
	buf->funcs.ctrl[Ctrl('L')] = center;
	buf->funcs.ctrl[Ctrl('M')] = newl;
	buf->funcs.ctrl[Ctrl('N')] = down;
	buf->funcs.ctrl[Ctrl('P')] = up;
	buf->funcs.ctrl[Ctrl('U')] = pgup;
	buf->funcs.ctrl[Ctrl('V')] = pgdown;
	buf->funcs.ctrl[Ctrl('X')] = cx;
	buf->funcs.ctrl[Ctrl('Z')] = suspend;
	buf->funcs.ctrl[Ctrl('[')] = esc;

	buf->funcs.up = up;
	buf->funcs.down = down;
	buf->funcs.left = left;
	buf->funcs.right = right;
	buf->funcs.home = bol;
	buf->funcs.end = eol;
	buf->funcs.pgdown = pgdown;
	buf->funcs.pgup = pgup;
	buf->funcs.delete = del;
	buf->funcs.f[2] = save;
	buf->funcs.f[3] = open_file;
	buf->funcs.f[4] = prevbuffer;
	buf->funcs.f[5] = nextbuffer;
	buf->funcs.f[8] = close_buffer;
	buf->funcs.f[10] = quit;

	return buf;
}

// Opens a file.
void
open_file(Editor *e) {
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
close_buffer(Editor *e) {
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
		free(txt);
		return;
	}
	if (fwrite(txt, 1, b->bytes, fd) != b->bytes) {
		snprintf(m, 1023, "Error saving %s", b->filename);
		msg(e, m);
	} else {
		msg(e, "wrote file");
		e->txtbuf->changed = 0;
	}
	fclose(fd);
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
	clear_window(e->txtbuf->win);
}

// Advances to the next buffer in the buffer list.
void
nextbuffer(Editor *e) {
	e->txtbuf = e->txtbuf->next;
	e->txtbuf->redisp = 1;
	clear_window(e->txtbuf->win);
}

void
msg(Editor *e, char *msg) {
	if (e->prompt)
		return;

	size_t len = strlen(msg);

	set_foreground(Yellow);
	display(e->prbuf->win, 0, 0, msg);
	for (size_t i = len; i <= e->txtbuf->win.columns; i++){
		display(e->prbuf->win, 0, i, " ");
	}
	reset_colors();
	e->msg = 1;
}

void
cx(Editor *e) {
	int c = getchar();

	switch(c) {
	case Ctrl('S'):
	case 's': save(e); break;
	case Ctrl('W'):
	case 'w': saveas(e); break;
	case Ctrl('K'):
	case 'k': close_buffer(e); break;
	case Ctrl('C'):
	case 'c': quit(e); break;
	case Ctrl('N'):
	case 'n': nextbuffer(e); break;
	case Ctrl('P'):
	case 'p': prevbuffer(e); break;
	case Ctrl('F'):
	case 'f': open_file(e); break;
	default: msg(e, "Sequence not bound");
	}
}

static void
csi(Editor *e) {
	int c = getchar();
	Func f = NULL;
	Functions funcs = e->current->funcs;
	int need_tilde = 1;

	if (c == '1') {
		c = getchar();
		switch (c) {
		case '5': f = funcs.f[5]; break;
		case '7': f = funcs.f[6]; break;
		case '8': f = funcs.f[7]; break;
		case '9': f = funcs.f[8]; break;
		default:
			msg(e, "Sequence not bound");
			return;
		}
	} else if (c == '2') {
		c = getchar();
		if (c == '~') {
			f = funcs.insert;
			need_tilde = 0;
		} else {
			switch (c) {
			case '0': f = funcs.f[9]; break;
			case '1': f = funcs.f[10]; break;
			case '3': f = funcs.f[11]; break;
			case '4': f = funcs.f[12]; break;
			default:
				msg(e, "Sequence not bound");
				return;
			}
		}
	} else {
		switch (c) {
		case '3': f = funcs.delete; break;
		case '5': f = funcs.pgup; break;
		case '6': f = funcs.pgdown; break;
		case 'A': f = funcs.up; need_tilde = 0; break;
		case 'B': f = funcs.down; need_tilde = 0; break;
		case 'C': f = funcs.right; need_tilde = 0; break;
		case 'D': f = funcs.left; need_tilde = 0; break;
		case 'H': f = funcs.home; need_tilde = 0; break;
		case 'F': f = funcs.end; need_tilde = 0; break;
		default:
			msg(e, "Sequence not bound");
			return;
		}
	}
	if (need_tilde) {
		c = getchar();
		if (c != '~') {
			msg(e, "Sequence not bound");
			return;
		}
	}
	if (f != NULL) {
		f(e);
		e->current->lastfunc = f;
	}
}

void
esc(Editor *e) {
	int c = getchar();
	Func f = NULL;
	Functions funcs = e->current->funcs;

	switch (c) {
	case 'O':
		c = getchar();

		switch (c) {
		case 'P': f = funcs.f[1]; break;
		case 'Q': f = funcs.f[2]; break;
		case 'R': f = funcs.f[3]; break;
		case 'S': f = funcs.f[4]; break;
		}
		break;
	case '[':
		csi(e);
		return;
	default:
		msg(e, "Sequence not bound");
		return;
	}
	f(e);
	e->current->lastfunc = f;
}
