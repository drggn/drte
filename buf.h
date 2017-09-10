struct Editor;
typedef void(*Func)(struct Editor *);

typedef struct Buffer{
	size_t col;
	size_t line;
	size_t curline;
	size_t off;
	size_t vis;
	size_t bytes;
	char *filename; // NULL if no file
	int redisp;
	int changed;
	Gapbuf *gbuf;
	WINDOW *win;
	Func lastfunc;
	Func funcs[32 + (KEY_UNDO - KEY_BREAK) + 1];

	struct Buffer *next;
	struct Buffer *prev;
}Buffer;

typedef struct Editor{
	int msg;
	int prompt;
	int stop;
	int cancel;
	Buffer *prbuf;
	char *promptstr;
	Buffer *txtbuf; // circular linked list
	Buffer *current;
}Editor;
