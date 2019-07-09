#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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

static void set_up_terminal(void);
static void sigwinch_handler(int n);
static void sigcont_handler(int n);
static void end(void);
static void make_prompt_buffer(Editor *e);
static void make_prompt_funcs(Editor *e);

int terminal;
struct termios old_terminal_config;
struct termios terminal_config;
Editor *editor;

static void
set_up_terminal(void) {
	tcgetattr(terminal, &terminal_config);
	old_terminal_config = terminal_config;
	cfmakeraw(&terminal_config);
	tcsetattr(terminal, TCSANOW, &terminal_config);
}

static void
sigwinch_handler(int n) {
	resize(editor);
}

static void
sigcont_handler(int n) {
	set_up_terminal();
	redraw(editor);
}

static void
end(void) {
	tcsetattr(terminal, TCSANOW, &old_terminal_config);
	reset_colors();
	printf("\033[2J\n"); // clear screen
	close(terminal);
}


static void
make_prompt_buffer(Editor *e) {
	e->prompt_buffer = xmalloc(sizeof(*(e->prompt_buffer)));
	memset(e->prompt_buffer, 0, sizeof(*(e->prompt_buffer)));
	e->prompt_buffer->gap_buffer = gbf_new(NULL, 0);
	resize_window(&e->prompt_buffer->window, 1, e->display.columns);
	move_window(&e->prompt_buffer->window, e->display.lines, 1);
	make_prompt_funcs(e);
}

static void
make_prompt_funcs(Editor *e) {
	Buffer *buf = e->prompt_buffer;

	buf->funcs.ctrl[Ctrl('A')] = uf_bol;
	buf->funcs.ctrl[Ctrl('B')] = uf_left;
	buf->funcs.ctrl[Ctrl('C')] = uf_cancel_loop;
	buf->funcs.ctrl[Ctrl('D')] = uf_delete;
	buf->funcs.ctrl[Ctrl('E')] = uf_eol;
	buf->funcs.ctrl[Ctrl('F')] = uf_right;
	buf->funcs.ctrl[Ctrl('G')] = uf_cancel_loop;
	buf->funcs.ctrl[Ctrl('H')] = uf_backspace;
	buf->funcs.ctrl[Ctrl('I')] = uf_tab;
	buf->funcs.ctrl[Ctrl('J')] = uf_stop_loop;
	buf->funcs.ctrl[Ctrl('M')] = uf_stop_loop;
	buf->funcs.ctrl[Ctrl('[')] = uf_escape;

	buf->funcs.left = uf_left;
	buf->funcs.right = uf_right;
	buf->funcs.home = uf_bol;
	buf->funcs.end = uf_eol;
	buf->funcs.delete = uf_delete;
}

int
main(int argc, char **argv) {
	Buffer *buf;
	size_t lines;

	setlocale(LC_ALL, "");

	terminal = open("/dev/tty", O_RDWR);
	if(!isatty(terminal)) {
		fprintf(stderr, "Can't open tty connection\n");
		exit(-1);
	}

	set_up_terminal();
	signal(SIGCONT, sigcont_handler);
	signal(SIGWINCH, sigwinch_handler);

	atexit(end);

	editor = xmalloc(sizeof(*editor));
	memset(editor, 0, sizeof(*editor));

	set_display_size(editor);

	if (editor->display.lines > 512) {
		lines = editor->display.lines;
	} else {
		lines = 512;
	}
	editor->line_length = xmalloc(sizeof(size_t) * lines);
	editor->max_lines = lines;

	for (size_t i = 1; i < argc; i++) {
		buf = newbuffer(editor, argv[i]);
		addbuffer(editor, buf, 1);
	}
	if (editor->text_buffer == NULL) {
		buf = newbuffer(editor, NULL);
		addbuffer(editor, buf, 0);
	}
	if (editor->text_buffer == NULL) {
		fprintf(stderr, "Init failed\n");
		return -1;
	}

	make_prompt_buffer(editor);
	resize_window(&editor->status_bar, 1, editor->display.columns);
	move_window(&editor->status_bar, editor->display.lines - 1, 1);

	editor->current = editor->text_buffer;
	loop(editor);
	return 0;
}
