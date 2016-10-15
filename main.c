#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ncurses.h>

#include "gapbuf.h"
#include "xmalloc.h"
#include "buf.h"
#include "funcs.h"

// Wrapper for atexit.
static void
_endwin(void){
	endwin();
}

static void
mkprfuncs(Editor *e){
	memset(e->prfuncs, 0, (32 + (KEY_UNDO - KEY_BREAK) + 1) * 8);
	e->prfuncs[Ctrl('A')] = bol;
	e->prfuncs[Ctrl('B')] = left;
	e->prfuncs[Ctrl('D')] = del;
	e->prfuncs[Ctrl('E')] = eol;
	e->prfuncs[Ctrl('F')] = right;
	e->prfuncs[Ctrl('H')] = bksp;
	// TODO: handle newline
	e->prfuncs[Ncur(KEY_LEFT)] = left;
	e->prfuncs[Ncur(KEY_RIGHT)] = right;
	e->prfuncs[Ncur(KEY_HOME)] = bol;
	e->prfuncs[Ncur(KEY_END)] = eol;
	e->prfuncs[Ncur(KEY_DC)] = del;
	e->prfuncs[Ncur(KEY_BACKSPACE)] = bksp;
}

int
main(int argc, char **argv){
	Editor *e;
	Buffer *buf;
	Func funcs[32 + (KEY_UNDO - KEY_BREAK) + 1] = {0};
	char *txt;

	setlocale(LC_ALL, "");

	atexit(_endwin);
	initscr();
	noecho();
	raw();
	signal(SIGCONT, SIG_DFL);

	e = xmalloc(sizeof(*e));
	e->msg = 0;
	e->current = NULL;

	e->bar = newwin(1, 0, LINES - 1, 0);
	keypad(e->bar, TRUE);
	wattron(e->bar, A_REVERSE);

	for(size_t i = 1; i < argc; i++){
		buf = newbuffer(e, argv[i]);
		addbuffer(e, buf, 1);
	}
	if(e->current == NULL){
		buf = newbuffer(e, NULL);
		addbuffer(e, buf, 0);
	}
	if(e->current == NULL){
		fprintf(stderr, "Init failed\n");
		return -1;
	}
	e->prbuf = newbuffer(e, NULL);
	if(e->prbuf == NULL){
		fprintf(stderr, "Init failed\n");
		return -1;
	}
	mkprfuncs(e);

	funcs[Ctrl('A')] = bol;
	funcs[Ctrl('B')] = left;
	funcs[Ctrl('D')] = del;
	funcs[Ctrl('E')] = eol;
	funcs[Ctrl('F')] = right;
	funcs[Ctrl('H')] = bksp;
	funcs[Ctrl('I')] = tab;
	funcs[Ctrl('J')] = newl;
	funcs[Ctrl('M')] = newl;
	funcs[Ctrl('N')] = down;
	funcs[Ctrl('P')] = up;
	funcs[Ctrl('X')] = cx;
	funcs[Ctrl('Z')] = suspend;
	// TODO: handle newline
	funcs[Ncur(KEY_UP)] = up;
	funcs[Ncur(KEY_DOWN)] = down;
	funcs[Ncur(KEY_LEFT)] = left;
	funcs[Ncur(KEY_RIGHT)] = right;
	funcs[Ncur(KEY_HOME)] = bol;
	funcs[Ncur(KEY_END)] = eol;
	funcs[Ncur(KEY_DC)] = del;
	funcs[Ncur(KEY_ENTER)] = newl;
	funcs[Ncur(KEY_BACKSPACE)] = bksp;
	funcs[Ncur(KEY_STAB)] = tab;
	funcs[Ncur(KEY_F(2))] = save;
	funcs[Ncur(KEY_F(3))] = open;
	funcs[Ncur(KEY_F(4))] = prevbuffer;
	funcs[Ncur(KEY_F(5))] = nextbuffer;
	funcs[Ncur(KEY_F(8))] = close;
	funcs[Ncur(KEY_F(10))] = quit;

	txt = gbftxt(e->current->gbuf);
	mvwaddstr(e->current->win, 0, 0, txt);

 loop:
	if(e->msg){
		e->msg = 0;
	}else{
		char msg[COLS];
		snprintf(msg, COLS - 1, "(%ld,%ld) vis: %ld %ld/%ld: %s%s",
				 e->current->line, e->current->col, e->current->vis,
				 e->current->off, e->current->bytes,
				 e->current->filename ? e->current->filename : "Unnamed",
				 e->current->changed ? "*" : " ");

		mvwaddstr(e->bar, 0, 0, msg);
		for(size_t i = strlen(msg); i <= COLS; i++)
			waddstr(e->bar, " ");
	}
	wrefresh(e->bar);
	if(e->current->redisp){
		free(txt);
		wclear(e->current->win);
		txt = gbftxt(e->current->gbuf);
		mvwaddstr(e->current->win, 0, 0, txt + e->current->vis);
		wrefresh(e->current->win);
		e->current->redisp = 0;
	}
	wmove(e->current->win, e->current->curline, e->current->col);

	int c = wgetch(e->current->win);
	if((c >= 0 && c <= 31) || (c >= KEY_BREAK && c <= KEY_UNDO)){
		if(funcs[Code(c)] != NULL){
			funcs[Code(c)](e);
			e->current->lastfunc = funcs[Code(c)];
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
			msg(e, "Invalid input.");
			goto loop;
		}
		inp[0] = c;
		for(int i = 1; i < b; i++){
			c = wgetch(e->current->win);
			if((c & 0x00000080) == 0x00000080){
				inp[i] = c;
			}else{
				msg(e, "Invalid input");
				goto loop;
			}
		}
		ins(e->current, inp);
	}
	goto loop;
	// not reached
	return 0;
}

