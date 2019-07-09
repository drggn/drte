typedef struct {
	struct {
		size_t line;
		size_t column;
	} position;

	struct {
		size_t lines;
		size_t columns;
	} size;
} Window;

typedef enum {
	Black, Red, Green, Yellow, Blue, Magenta, Cyan, White
} Color;

struct Editor;

void display(Window win, size_t line, size_t column, char *text);
void clear_window(Window win);
void set_display_size(struct Editor *e);
void set_foreground(Color c);
void set_background(Color c);
void set_reverse(void);
void reset_colors(void);
void refresh(void);
void move_cursor(Window win, size_t line, size_t column);
void move_window(Window *win, size_t line, size_t column);
void resize_window(Window *win, size_t lines, size_t columns);
