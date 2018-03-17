#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include <ncurses.h>

#include "gapbuf.h"
#include "buf.h"
#include "utils.h"
#include "funcs.h"

// Wrapper for atexit.
static void
_endwin(void) {
	endwin();
}

static void
mkprfuncs(Editor *e) {
	Func *funcs = e->prbuf->funcs;
	memset(funcs, 0, (NCURSESKEYS * 8));
	funcs[Ctrl('A')] = bol;
	funcs[Ctrl('B')] = left;
	funcs[Ctrl('C')] = cancelloop;
	funcs[Ctrl('D')] = del;
	funcs[Ctrl('E')] = eol;
	funcs[Ctrl('F')] = right;
	funcs[Ctrl('G')] = cancelloop;
	funcs[Ctrl('H')] = bksp;
	funcs[Ctrl('I')] = tab;
	funcs[Ctrl('J')] = stoploop;
	funcs[Ctrl('M')] = stoploop;

	funcs[Ncur(KEY_LEFT)] = left;
	funcs[Ncur(KEY_RIGHT)] = right;
	funcs[Ncur(KEY_HOME)] = bol;
	funcs[Ncur(KEY_END)] = eol;
	funcs[Ncur(KEY_DC)] = del;
	funcs[Ncur(KEY_BACKSPACE)] = bksp;
	funcs[Ncur(KEY_ENTER)] = stoploop;
}

static void
mkprbuf(Editor *e) {
	e->prbuf = xmalloc(sizeof(*(e->prbuf)));
	memset(e->prbuf, 0, sizeof(*(e->prbuf)));
	e->prbuf->gbuf = gbfnew(NULL, 0);
	e->prbuf->win = newwin(1, 0, LINES - 1, 0);
	keypad(e->prbuf->win, TRUE);
	wattron(e->prbuf->win, A_REVERSE);
	mkprfuncs(e);
}

int
main(int argc, char **argv) {
	Editor *e;
	Buffer *buf;

	setlocale(LC_ALL, "");

	atexit(_endwin);
	initscr();
	noecho();
	raw();
	signal(SIGCONT, SIG_DFL);

	e = xmalloc(sizeof(*e));
	memset(e, 0, sizeof(*e));
	e->txtbuf = NULL;

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

	loop(e);
	return 0;
}
