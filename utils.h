#define width(c) ((c) == '\t' ? 8 : 1)

bool streql(char *s, char *t);
void *xmalloc(size_t size);
int is_newline(unsigned char c);
int is_whitespace(unsigned char c);
size_t bytes(unsigned char c);
