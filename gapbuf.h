typedef struct{
	char *buf;
	char *gst;
	char *sec;
	char *bend;
}Gapbuf;

#define gaplen(gbuf) (gbuf->sec - gbuf->gst)
#define fstlen(gbuf) (gbuf->gst - gbuf->buf)
#define seclen(gbuf) (gbuf->bend - gbuf->sec)

Gapbuf *gbfnew(char *file, size_t size);
void gbffree(Gapbuf *buf);
char *gbftxt(Gapbuf *buf);
void gbfins(Gapbuf *buf, char *s, size_t off);
void gbfdel(Gapbuf *buf, size_t off, size_t n);
void gbfclear(Gapbuf *buf);
char gbfat(Gapbuf *buf, size_t off);

