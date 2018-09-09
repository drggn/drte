#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "gapbuf.h"
#include "display.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

int term;
struct termios old;
struct termios ti;
Editor *e;

static void
end(void) {
	tcsetattr(term, TCSANOW, &old);
	reset_colors();
	printf("\033[2J\n"); // TODO
	close(term);
}

static void
mkprfuncs(Editor *e) {
	Buffer *buf = e->prbuf;

	buf->funcs.ctrl[Ctrl('A')] = bol;
	buf->funcs.ctrl[Ctrl('B')] = left;
	buf->funcs.ctrl[Ctrl('C')] = cancelloop;
	buf->funcs.ctrl[Ctrl('D')] = del;
	buf->funcs.ctrl[Ctrl('E')] = eol;
	buf->funcs.ctrl[Ctrl('F')] = right;
	buf->funcs.ctrl[Ctrl('G')] = cancelloop;
	buf->funcs.ctrl[Ctrl('H')] = bksp;
	buf->funcs.ctrl[Ctrl('I')] = tab;
	buf->funcs.ctrl[Ctrl('J')] = stoploop;
	buf->funcs.ctrl[Ctrl('M')] = stoploop;
	buf->funcs.ctrl[Ctrl('[')] = esc;

	buf->funcs.left = left;
	buf->funcs.right = right;
	buf->funcs.home = bol;
	buf->funcs.end = eol;
	buf->funcs.delete = del;
}

static void
mkprbuf(Editor *e) {
	e->prbuf = xmalloc(sizeof(*(e->prbuf)));
	memset(e->prbuf, 0, sizeof(*(e->prbuf)));
	e->prbuf->gbuf = gbfnew(NULL, 0);
	resize_window(&e->prbuf->win, 1, e->display.columns);
	move_window(&e->prbuf->win, e->display.lines, 1);
	mkprfuncs(e);
}

static void
winch(int n) {
	resize(e);
}
static void
cont(int n) {
	tcgetattr(term, &ti);
	old = ti;
	cfmakeraw(&ti);
	tcsetattr(term, TCSANOW, &ti);
	redraw(e);
}

int
main(int argc, char **argv) {
	Buffer *buf;
	size_t lines;

	setlocale(LC_ALL, "");

	term = open("/dev/tty", O_RDWR);
	if(!isatty(term)) {
		fprintf(stderr, "Can't open tty connection\n");
		exit(-1);
	}

	tcgetattr(term, &ti);
	old = ti;
	cfmakeraw(&ti);
	tcsetattr(term, TCSANOW, &ti);

	signal(SIGCONT, cont);
	signal(SIGWINCH, winch);

	atexit(end);

	e = xmalloc(sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->txtbuf = NULL;

	set_display_size(e);

	if (e->display.lines > 512) {
		lines = e->display.lines;
	} else {
		lines = 512;
	}
	e->linelength = xmalloc(sizeof(size_t) * lines);
	e->maxlines = lines;

	for (size_t i = 1; i < argc; i++) {
		buf = newbuffer(e, argv[i]);
		addbuffer(e, buf, 1);
	}
	if (e->txtbuf == NULL) {
		buf = newbuffer(e, NULL);
		addbuffer(e, buf, 0);
	}
	if (e->txtbuf == NULL) {
		fprintf(stderr, "Init failed\n");
		return -1;
	}

	mkprbuf(e);
	resize_window(&e->statusbar, 1, e->display.columns);
	move_window(&e->statusbar, e->display.lines - 1, 1);

	e->current = e->txtbuf;
	loop(e);
	return 0;
}
