struct Editor;
typedef void(*Func)(struct Editor *);

#define NCURSESKEYS ((KEY_MAX - KEY_MIN) - 2)

typedef struct Buffer{
	size_t line;
	size_t curcol;
	size_t curline;
	size_t off;
	size_t startvis;
	size_t viscol;
	size_t bytes;
	char *filename; // NULL if no file
	int redisp;
	int changed;
	Gapbuf *gbuf;
	WINDOW *win;
	Func lastfunc;
	Func funcs[NCURSESKEYS];

	struct Buffer *next;
	struct Buffer *prev;
}Buffer;

typedef struct Editor{
	int msg;
	int prompt;
	int stop;
	int cancel;
	size_t *linelength;
	size_t maxlines;

	Buffer *prbuf;
	char *promptstr;
	Buffer *txtbuf; // circular linked list
	Buffer *current;
}Editor;
