struct Editor;
typedef void(*Func)(struct Editor *);

typedef struct {
	Func ctrl[32];
	Func f[13]; // f[0] is always NULL
	Func up;
	Func down;
	Func left;
	Func right;
	Func home;
	Func end;
	Func pgup;
	Func pgdown;
	Func insert;
	Func delete;
	// TODO: more
}Functions;

typedef struct Buffer {
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
	Window win;
	Func lastfunc;
	Functions funcs;

	struct Buffer *next;
	struct Buffer *prev;
} Buffer;

typedef struct Editor {
	int msg;
	int prompt;
	int stop;
	int cancel;
	size_t *linelength;
	size_t maxlines;

	struct {
		size_t lines;
		size_t columns;
	} display;

	Buffer *prbuf;
	char *promptstr;
	Window statusbar;
	Buffer *txtbuf; // circular linked list
	Buffer *current;
} Editor;
