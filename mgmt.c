#include <stdbool.h>
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

static void scroll_horizontal(Editor *e);
void stop_loop(Editor *e);
void cancel_loop(Editor *e);

// Move the screen left/right.
static void
scroll_horizontal(Editor *e) {
	Buffer *b = e->text_buffer;

	if ((b->cursor.column < (b->window.size.columns / 2))) {
		e->text_buffer->first_visible_column = 0;
	} else {
		b->first_visible_column =
			b->cursor.column - (b->window.size.columns / 2);
	}
}

void
loop(Editor *e) {
	char *text = gbf_text(e->text_buffer->gap_buffer);
	char *ptext = gbf_text(e->prompt_buffer->gap_buffer);
	Buffer *tb = e->text_buffer;
	Buffer *pb = e->prompt_buffer;

	do {
		if (e->message) {
			e->message = false;
		} else if (e->prompt) {
			size_t coloff = strlen(e->prompt_string);

			if (pb->cursor.column == 0)
				pb->cursor.column = coloff;

			clear_window(pb->window);

			free(ptext);
			ptext = gbf_text(pb->gap_buffer);

			set_foreground(Yellow);
			display(pb->window, 0, 0, e->prompt_string);
			reset_colors();
			display(pb->window, 0, coloff, ptext);
		} else {
			clear_window(pb->window);
		}

		// draw status_bar
		char status[pb->window.size.columns + 1];
		snprintf(status, pb->window.size.columns,
				 "(%ld,%ld) startvis: %ld %ld/%ld: %s%s",
				 tb->line, tb->cursor.column + tb->first_visible_column,
				 tb->first_visible_char,
				 tb->offset, tb->bytes,
				 tb->file_name ? tb->file_name : "Unnamed",
				 tb->changed ? "*" : " ");

		set_background(Blue);
		set_foreground(Black);
		display(e->status_bar, 0, 0, status);
		for (size_t i = strlen(status); i <= pb->window.size.columns; i++)
			display(e->status_bar, 0, 0 + i, " ");
		reset_colors();

		if (tb->cursor.column >= tb->first_visible_column + tb->window.size.columns
			|| tb->cursor.column < tb->first_visible_column) {
			scroll_horizontal(e);
			redraw(e);
		}
		// Draw the text buffer.
		if (tb->redisplay) {
			size_t line = 0;
			size_t col = 0;
			int last_white_char = -1;

			free(text);
			clear_window(tb->window);

			text = gbf_text(tb->gap_buffer);
			size_t current = tb->first_visible_char;

			while (text[current]) {
				size_t wid;
				char cp[5] = {0}; // Code point
				int size = bytes(text[current]);

				for (int i = 0; i < size; i++, current++) {
					cp[i] = text[current];
				}
				wid = width(cp[0]);

				if (tb->first_visible_column <= col &&
					col < (tb->first_visible_column + tb->window.size.columns)) {
					display(tb->window, line, col - tb->first_visible_column, cp);
				}
				if (is_whitespace(cp[0])) {
					if (last_white_char == -1) {
						last_white_char = col;
					}
				} else {
					last_white_char = -1;
				}

				if ((cp[0] == '\n') || (text[current] == '\0')) {
					e->line_length[line] = col;

					if (last_white_char != -1) {
						// draw trailing whitespace
						int c = last_white_char;
						if ((text[current] == '\0') && (text[current - 1] != '\n'))
							col += wid;
						move_cursor(tb->window, line, c);
						set_background(Red);
						while (c != col) {
							display(tb->window, line, c, " ");
							c++;
						}
						reset_colors();
					}
					if (cp[0] == '\n') {
						line++;
						col = 0;
						last_white_char = -1;
					}
				} else  {
					col += wid;
				}
				if (line >= tb->window.size.lines)
					break;
			}
			e->text_buffer->redisplay = false;
		}
		move_cursor(e->current->window, e->current->cursor.line,
					e->current->cursor.column - e->current->first_visible_column);
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
					message(e, "Invalid input");
					continue;
				}
			}
			insert(e, inp);
		}
	} while (!e->stop);
}

// Redraw the display
void
redraw(Editor *e) {
	e->current->redisplay = true;
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
		resize_window(&buf->window, lines - 2, e->display.columns);
		buf = buf->next;
	} while (buf != b);

	resize_window(&e->prompt_buffer->window, 1, e->display.columns);
	move_window(&e->prompt_buffer->window, lines, 1);
	move_window(&e->status_bar, lines - 1, 1);

	if (lines > e->max_lines) {
		if (realloc(e->line_length, lines * 2)) {
			e->max_lines = lines * 2;
		} else {
			fprintf(stderr, "Out of memory");
			exit(-1);
		}
	}
	if (b->cursor.line >= lines) {
		uf_center(e);
	}
	scroll_horizontal(e);

	// TODO: screen is not redrawn automatically, because main loop blocks
	redraw(e);
}

// Displays prompt in the status bar. Returns
// the text entered by the user.
static char *
prompt(Editor *e, char *prompt) {
	e->prompt = true;
	e->prompt_string = prompt;
	e->prompt_buffer->cursor.column = 0;
	e->prompt_buffer->offset = 0;
	e->prompt_buffer->bytes = 0;
	e->current = e->prompt_buffer;
	gbf_clear(e->prompt_buffer->gap_buffer);


	loop(e);

	e->prompt = false;
	e->stop = false;
	e->current = e->text_buffer;
	clear_window(e->prompt_buffer->window);

	if (e->cancel) {
		e->cancel = false;
		return NULL;
	} else {
		return gbf_text(e->prompt_buffer->gap_buffer);
	}
}

// Adds buffer b to the editor. If before is true,
// b is inserted before the current buffer. Else after.
void
addbuffer(Editor *e, Buffer *buf, int before) {
	if (buf == NULL)
		return;
	if (e->text_buffer == NULL) {
		e->text_buffer = buf;
		buf->next = buf;
		buf->prev = buf;
	} else if (before) {
		buf->next = e->text_buffer;
		buf->prev = e->text_buffer->prev;
		buf->prev->next = buf;
		e->text_buffer->prev = buf;
	} else {
		buf->next = e->text_buffer->next;
		e->text_buffer->next = buf;
		buf->prev = e->text_buffer;
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
			message(e, "Can't stat file.");
			return NULL;
		} else {
			size = 0;
		}
	} else {
		size = st.st_size;
	}
	buf = xmalloc(sizeof(*buf));
	memset(buf, 0, sizeof(*buf));
	resize_window(&buf->window, e->display.lines - 2, e->display.columns);
	move_window(&buf->window, 1, 1);

	buf->bytes = size;
	buf->gap_buffer = gbf_new(file, size);
	if (file != NULL) {
		buf->file_name = xmalloc(strlen(file) + 1);
		strcpy(buf->file_name, file);
	}
	buf->redisplay = true;

	buf->funcs.ctrl[Ctrl('A')] = uf_bol;
	buf->funcs.ctrl[Ctrl('B')] = uf_left;
	buf->funcs.ctrl[Ctrl('D')] = uf_delete;
	buf->funcs.ctrl[Ctrl('E')] = uf_eol;
	buf->funcs.ctrl[Ctrl('F')] = uf_right;
	buf->funcs.ctrl[Ctrl('H')] = uf_backspace;
	buf->funcs.ctrl[Ctrl('I')] = uf_tab;
	buf->funcs.ctrl[Ctrl('J')] = uf_newline;
	buf->funcs.ctrl[Ctrl('L')] = uf_center;
	buf->funcs.ctrl[Ctrl('M')] = uf_newline;
	buf->funcs.ctrl[Ctrl('N')] = uf_down;
	buf->funcs.ctrl[Ctrl('P')] = uf_up;
	buf->funcs.ctrl[Ctrl('U')] = uf_page_up;
	buf->funcs.ctrl[Ctrl('V')] = uf_page_down;
	buf->funcs.ctrl[Ctrl('X')] = uf_cx;
	buf->funcs.ctrl[Ctrl('Z')] = uf_suspend;
	buf->funcs.ctrl[Ctrl('[')] = uf_escape;

	buf->funcs.up = uf_up;
	buf->funcs.down = uf_down;
	buf->funcs.left = uf_left;
	buf->funcs.right = uf_right;
	buf->funcs.home = uf_bol;
	buf->funcs.end = uf_eol;
	buf->funcs.pgdown = uf_page_down;
	buf->funcs.pgup = uf_page_up;
	buf->funcs.delete = uf_delete;
	buf->funcs.f[2] = uf_save;
	buf->funcs.f[3] = uf_open_file;
	buf->funcs.f[4] = uf_previous_buffer;
	buf->funcs.f[5] = uf_next_buffer;
	buf->funcs.f[8] = uf_close_buffer;
	buf->funcs.f[10] = uf_quit;

	return buf;
}

// Opens a file.
void
uf_open_file(Editor *e) {
	char *name = prompt(e, "Name: ");
	Buffer *b = e->text_buffer;
	if (name == NULL)
		return;
	// check for existng buffer
	do {
		if (b->file_name != NULL && streql(b->file_name, name)) {
			e->text_buffer = b;
			free(name);
			e->text_buffer->redisplay = true;
			return;
		}
		b = b->next;
	} while (b != e->text_buffer);

	Buffer *buf = newbuffer(e, name);
	addbuffer(e, buf, 0);
	free(name);
	uf_next_buffer(e);
}

// Closes the current buffer.
void
uf_close_buffer(Editor *e) {
	Buffer *b = e->text_buffer;
	Buffer *new;
	char msg[1024];
	char *txt;

	if (b->changed) {
		snprintf(msg, 1023, "%s has changed. Save? ",
				 e->text_buffer->file_name);
		txt = prompt(e, msg);
		if (txt == NULL)
			return;
		if (streql(txt, "y") || streql(txt, "yes")) {
			uf_save(e);
		}
	}
	if (b->file_name != NULL)
		free(b->file_name);
	gbf_free(b->gap_buffer);

	if (b->next == b) {
		free(b);
		e->text_buffer = NULL;
		new = newbuffer(e, NULL);
		addbuffer(e, new, 0);
		uf_next_buffer(e);
	} else {
		b->next->prev = b->prev;
		b->prev->next = b->next;
		uf_next_buffer(e);
		free(b);
	}
}

// Saves a file.
void
uf_save(Editor *e) {
	FILE *fd;
	char *txt;
	char m[1024];
	Buffer *b = e->text_buffer;

	if (b->changed == false) {
		message(e, "File hasn't changed");
		return;
	}
	if (b->file_name == NULL) {
		uf_save_as(e);
		return;
	}

	txt = gbf_text(b->gap_buffer);

	if ((fd = fopen(b->file_name, "w")) == NULL) {
		snprintf(m, 1023, "Can't open %s", b->file_name);
		message(e, m);
		free(txt);
		return;
	}
	if (fwrite(txt, 1, b->bytes, fd) != b->bytes) {
		snprintf(m, 1023, "Error saving %s", b->file_name);
		message(e, m);
	} else {
		message(e, "wrote file");
		e->text_buffer->changed = false;
	}
	fclose(fd);
	free(txt);
}

// Saves a file under a different name.
void
uf_save_as(Editor *e) {
	char* name = prompt(e, "File_Name: ");
	if (name == NULL)
		return;
	e->text_buffer->file_name = name;
	uf_save(e);
}

void
uf_stop_loop(Editor *e) {
	e->stop = true;
}

void
uf_cancel_loop(Editor *e) {
	e->cancel = true;
}

// Quits drte.
void
uf_quit(Editor *e) {
	char *txt;
	char m[1024];
	Buffer *first = e->text_buffer;

	do {
		if (e->text_buffer->file_name != NULL && e->text_buffer->changed == true) {
			snprintf(m, 1023, "Save %s? ", e->text_buffer->file_name);
			txt = prompt(e, m);
			if (txt == NULL)
				return;
			if (streql(txt, "yes") || streql(txt, "y"))
				uf_save(e);
			free(txt);
		}
		e->text_buffer = e->text_buffer->next;
	} while (e->text_buffer != first);

	e->stop = true;
}

void
uf_suspend(Editor *e) {
	kill(0, SIGTSTP);
}

// Selects the previous buffer in the buffer list.
void
uf_previous_buffer(Editor *e) {
	e->text_buffer = e->text_buffer->prev;
	e->text_buffer->redisplay = true;
	clear_window(e->text_buffer->window);
}

// Advances to the next buffer in the buffer list.
void
uf_next_buffer(Editor *e) {
	e->text_buffer = e->text_buffer->next;
	e->text_buffer->redisplay = true;
	clear_window(e->text_buffer->window);
}

void
message(Editor *e, char *msg) {
	if (e->prompt)
		return;

	size_t len = strlen(msg);

	set_foreground(Yellow);
	display(e->prompt_buffer->window, 0, 0, msg);
	for (size_t i = len; i <= e->text_buffer->window.size.columns; i++){
		display(e->prompt_buffer->window, 0, i, " ");
	}
	reset_colors();
	e->message = true;
}

void
uf_cx(Editor *e) {
	int c = getchar();

	switch(c) {
	case Ctrl('S'):
	case 's': uf_save(e); break;
	case Ctrl('W'):
	case 'w': uf_save_as(e); break;
	case Ctrl('K'):
	case 'k': uf_close_buffer(e); break;
	case Ctrl('C'):
	case 'c': uf_quit(e); break;
	case Ctrl('N'):
	case 'n': uf_next_buffer(e); break;
	case Ctrl('P'):
	case 'p': uf_previous_buffer(e); break;
	case Ctrl('F'):
	case 'f': uf_open_file(e); break;
	default: message(e, "Sequence not bound");
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
			message(e, "Sequence not bound");
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
				message(e, "Sequence not bound");
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
			message(e, "Sequence not bound");
			return;
		}
	}
	if (need_tilde) {
		c = getchar();
		if (c != '~') {
			message(e, "Sequence not bound");
			return;
		}
	}
	if (f != NULL) {
		f(e);
		e->current->lastfunc = f;
	}
}

void
uf_escape(Editor *e) {
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
		message(e, "Sequence not bound");
		return;
	}
	f(e);
	e->current->lastfunc = f;
}
