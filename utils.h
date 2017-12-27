#define width(c) ((c) == '\t' ? 8 : 1)

void *xmalloc(size_t size);
int isnewline(unsigned char c);
int iswhitespace(unsigned char c);
size_t bytes(unsigned char c);
