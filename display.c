#include <stdio.h>
#include <sys/ioctl.h>

#include "gapbuf.h"
#include "display.h"
#include "buf.h"

void
display(Window win, size_t line, size_t column, char *text) {
	move_cursor(win, line, column);
	printf("%s", text);
}

void
clear_window(Window win) {
	for (int i = 0; i < win.lines; i++) {
		move_cursor(win, i, 0);
		printf("\033[2K");
	}
}

void
set_display_size(Editor *e) {
	struct winsize winsz;

	ioctl(1, TIOCGWINSZ, &winsz);
	e->display.lines = winsz.ws_row;
	e->display.columns = winsz.ws_col;
}

void
set_foreground(Color c) {
	printf("\033[3%dm", c);
}

void
set_background(Color c) {
	printf("\033[4%dm", c);
}

void
set_reverse(void) {
	printf("\033[7m");
}

void
reset_colors(void) {
	printf("\033[0m");
}

void
refresh(void) {
	fflush(stdout);
}

void
move_cursor(Window win, size_t line, size_t column) {
	printf("\033[%ld;%ldH", win.ln + line, win.col + column);
	fflush(stdout);
}

void
move_window(Window *win, size_t line, size_t column) {
	win->ln = line;
	win->col = column;
}

void
resize_window(Window *win, size_t lines, size_t columns) {
	win->lines = lines;
	win->columns = columns;
}
