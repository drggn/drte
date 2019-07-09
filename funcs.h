#define Ctrl(c) ((c) - 64)

// In edit.c
void insert(Editor *e, char *s);
void uf_tab(Editor *e);
void uf_newline(Editor *e);
void uf_delete(Editor *e);
void uf_backspace(Editor *e);

// In move.c
void uf_center(Editor *e);
void uf_left(Editor *e);
void uf_right(Editor *e);
void uf_up(Editor *e);
void uf_down(Editor *e);
void uf_bol(Editor *e);
void uf_eol(Editor *e);
void uf_page_down(Editor *e);
void uf_page_up(Editor *e);

// In mgmt.c
void loop(Editor *e);
void uf_redraw(Editor *e);
void resize(Editor *e);
void addbuffer(Editor *e, Buffer *buf, int before);
Buffer *newbuffer(Editor *e, char *file);
void uf_open_file(Editor *e);
void uf_close_buffer(Editor *e);
void uf_save(Editor *e);
void uf_save_as(Editor *e);
void uf_stop_loop(Editor *e);
void uf_cancel_loop(Editor *e);
void uf_quit(Editor *e);
void uf_suspend(Editor *e);
void uf_previous_buffer(Editor *e);
void uf_next_buffer(Editor *e);
void message(Editor *e, char *msg);
void uf_cx(Editor *e);
void uf_escape(Editor *e);
