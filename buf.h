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

	struct Buffer *next;
	struct Buffer *prev;
}Buffer;

typedef struct Editor{
	WINDOW *bar;
	int msg;
	Buffer *prbuf;
	Func prfuncs[32 + (KEY_UNDO - KEY_BREAK) + 1];
	Buffer *current; // circular linked list
}Editor;
