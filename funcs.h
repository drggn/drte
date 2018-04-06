#define Ctrl(c) ((c) - 64)
#define Ncur(c) (31 + ((c) - KEY_MIN))
#define Code(c) ((c) >= 0 && (c) <= 31 ? (c) : Ncur((c)))

#define LineWrap(col) ((col) >= COLS - 1)

// In edit.c
void ins(Editor *e, char *s);
void tab(Editor *e);
void newl(Editor *e);
void del(Editor *e);
void bksp(Editor *e);

// In move.c
void forwoff(Buffer *b);
void backoff(Buffer *b);
void center(Editor *e);
void left(Editor *e);
void right(Editor *e);
void up(Editor *e);
void down(Editor *e);
void bol(Editor *e);
void eol(Editor *e);
void pgdown(Editor *e);
void pgup(Editor *e);

// In mgmt.c
void loop(Editor *e);
void redraw(Editor *e);
void addbuffer(Editor *e, Buffer *buf, int before);
Buffer *newbuffer(Editor *e, char *file);
void open(Editor *e);
void close(Editor *e);
void save(Editor *e);
void saveas(Editor *e);
void stoploop(Editor *e);
void cancelloop(Editor *e);
void quit(Editor *e);
void suspend(Editor *e);
void prevbuffer(Editor *e);
void nextbuffer(Editor *e);
void msg(Editor *e, char *msg);
void cx(Editor *e);
