#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>

#include "xmalloc.h"
#include "gapbuf.h"

#define GAPINCR 10
#define UTF8MAX 4
#define MINGAPSIZE (UTF8MAX + 1)

// Creates a new gapbuffer. Takes a filename and the size of it's contents.
Gapbuf *
gbfnew(char *file, size_t size){
	Gapbuf *buf;
	FILE *fd;

	buf = xmalloc(sizeof(*buf));
	buf->buf = xmalloc(GAPINCR + size + 1);
	buf->gst = buf->buf;
	buf->sec = buf->gst + GAPINCR;
	buf->bend = buf->sec + size;
	*(buf->bend) = '\0';

	if(size != 0){
		if((fd = fopen(file, "r")) == NULL)
			exit(-1); // TODO: error handling
		if(fread(buf->sec, size, 1, fd) != 1)
			exit(-1); // TODO: error handling
		// FIXME: fclose everywhere
		fclose(fd);
	}
	return buf;
}

// Frees buf.
void
gbffree(Gapbuf *buf){
	free(buf->buf);
	free(buf);
}

// Returns the contents of buf.
char *
gbftxt(Gapbuf *buf){
	size_t flen = fstlen(buf);
	size_t slen = seclen(buf);
	char *str = xmalloc(flen + slen + 1);

	if(flen == 0){
		str[0] = '\0';
	}else{
		strncpy(str, buf->buf, flen);
		str[flen] = '\0';
	}
	if(slen > 0){
		strcat(str, buf->sec);
	}
	return str;
}

// Moves gap to position off.
static void
mvgap(Gapbuf *buf, size_t off){
	size_t len;
	char *pos = buf->buf + off;

	if(pos == buf->gst){
		return;
	}
	if(pos > buf->gst){
		// cursor after gap
		pos += gaplen(buf);
		len = (pos - buf->sec);
		memmove(buf->gst, buf->sec, len);
		buf->gst += len;
		buf->sec += len;
	}else{
		// cursor before gap
		len = buf->gst - pos;
		buf->gst -= len;
		buf->sec -= len;
		memmove(buf->sec, pos, len);
	}
}

// Expands the gap by GAPINCR.
static void
expgap(Gapbuf *buf){
	size_t flen = fstlen(buf);
	size_t slen = seclen(buf);
	size_t glen = gaplen(buf);
	size_t newsize = (buf->bend - buf->buf) + GAPINCR + 1;
	void *new = realloc(buf->buf, newsize);

	if(new == NULL){
		// TOOO: handle error
		exit(-1);
	}else{
		buf->buf = new;
		buf->gst = buf->buf + flen;
		buf->sec = buf->gst + glen + GAPINCR;
		buf->bend = buf->sec + slen;
		if(slen != 0){
			memmove(buf->sec, buf->gst + glen, slen);
		}
		*(buf->bend) = '\0';
	}
}

// Inserts s into buf.
void
gbfins(Gapbuf *buf, char *s, size_t off){
	mvgap(buf, off);
	if(gaplen(buf) <= MINGAPSIZE)
		expgap(buf);
	while(*s)
		*(buf->gst++) = *s++;
}

// Delete n bytes after off.
void
gbfdel(Gapbuf *buf, size_t off, size_t n){
	mvgap(buf, off);
	if(buf->sec == buf->bend)
		return;
	buf->sec += n;
}

// Clears the gapbuffer (deletes all text).
void
gbfclear(Gapbuf *buf){
	buf->gst = buf->buf;
	buf->sec = buf->bend;
}

// Returns the byte at position off.
char
gbfat(Gapbuf *buf, size_t off){
	if(buf->buf + off < buf->gst)
		return *(buf->buf + off);
	else
		return *(buf->buf + off + gaplen(buf));
}
