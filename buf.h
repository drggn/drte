typedef struct Editor Editor;
typedef void (*Func)(Editor *);

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
} Functions;

typedef struct Buffer {
	char *file_name; // NULL if no file
	size_t bytes;
	size_t offset;
	size_t line;

	struct {
		size_t line;
		size_t column;
	} cursor;

	size_t first_visible_char;
	size_t first_visible_column;
	bool redisplay;
	bool changed;
	GapBuffer *gap_buffer;
	Window window;
	Func lastfunc;
	Functions funcs;

	struct Buffer *next;
	struct Buffer *prev;
} Buffer;

struct Editor {
	bool message;
	bool prompt;
	bool stop;
	bool cancel;
	size_t *line_length;
	size_t max_lines;

	struct {
		size_t lines;
		size_t columns;
	} display;

	Buffer *prompt_buffer;
	char *prompt_string;
	Window status_bar;
	Buffer *text_buffer; // circular linked list
	Buffer *current;
};
