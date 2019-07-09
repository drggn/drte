#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#include "utils.h"
#include "gapbuf.h"

struct GapBuffer {
	char *first_part;
	char *gap_start;
	char *second_part;
	char *end;
};

static void move_gap(GapBuffer *gbuf, size_t offset);
static void expand_gap(GapBuffer *gbuf);

#define gap_length(gbuf) (gbuf->second_part - gbuf->gap_start)
#define first_part_length(gbuf) (gbuf->gap_start - gbuf->first_part)
#define second_part_length(gbuf) (gbuf->end - gbuf->second_part)

#define GAP_INCREMENT 10
#define UTF8_MAX_LENGTH 4
#define MIN_GAP_SIZE (UTF8_MAX_LENGTH + 1)

// Creates a new gapbuffer. Takes a filename and the size of it's contents.
GapBuffer *
gbf_new(char *file_name, size_t size) {
	GapBuffer *gbuf;
	FILE *fd;

	gbuf = xmalloc(sizeof(*gbuf));
	gbuf->first_part = xmalloc(GAP_INCREMENT + size + 1);
	gbuf->gap_start = gbuf->first_part;
	gbuf->second_part = gbuf->gap_start + GAP_INCREMENT;
	gbuf->end = gbuf->second_part + size;
	*(gbuf->end) = '\0';

	if (size != 0) {
		if ((fd = fopen(file_name, "r")) == NULL)
			exit(-1); // TODO: error handling
		if (fread(gbuf->second_part, size, 1, fd) != 1)
			exit(-1); // TODO: error handling
		// FIXME: fclose everywhere
		fclose(fd);
	}
	return gbuf;
}

// Frees buf.
void
gbf_free(GapBuffer *gbuf) {
	free(gbuf->first_part);
	free(gbuf);
}

// Returns the contents of buf.
char *
gbf_text(GapBuffer *gbuf) {
	size_t flen = first_part_length(gbuf);
	size_t slen = second_part_length(gbuf);
	char *str = xmalloc(flen + slen + 1);

	if (flen == 0) {
		str[0] = '\0';
	} else {
		strncpy(str, gbuf->first_part, flen);
		str[flen] = '\0';
	}
	if (slen > 0) {
		strcat(str, gbuf->second_part);
	}
	return str;
}

// Moves gap to position off.
static void
move_gap(GapBuffer *gbuf, size_t offset) {
	size_t len;
	char *pos = gbuf->first_part + offset;

	if (pos == gbuf->gap_start) {
		return;
	}
	if (pos > gbuf->gap_start) {
		// cursor after gap
		pos += gap_length(gbuf);
		len = (pos - gbuf->second_part);
		memmove(gbuf->gap_start, gbuf->second_part, len);
		gbuf->gap_start += len;
		gbuf->second_part += len;
	} else {
		// cursor before gap
		len = gbuf->gap_start - pos;
		gbuf->gap_start -= len;
		gbuf->second_part -= len;
		memmove(gbuf->second_part, pos, len);
	}
}

// Expands the gap.
static void
expand_gap(GapBuffer *gbuf) {
	size_t flen = first_part_length(gbuf);
	size_t slen = second_part_length(gbuf);
	size_t glen = gap_length(gbuf);
	size_t new_size = (gbuf->end - gbuf->first_part) + GAP_INCREMENT + 1;
	void *new = realloc(gbuf->first_part, new_size);

	if (new == NULL) {
		// TOOO: handle error
		exit(-1);
	} else {
		gbuf->first_part = new;
		gbuf->gap_start = gbuf->first_part + flen;
		gbuf->second_part = gbuf->gap_start + glen + GAP_INCREMENT;
		gbuf->end = gbuf->second_part + slen;
		if (slen != 0) {
			memmove(gbuf->second_part, gbuf->gap_start + glen, slen);
		}
		*(gbuf->end) = '\0';
	}
}

// Inserts s into buf.
void
gbf_insert(GapBuffer *gbuf, char *s, size_t offset) {
	size_t len = strlen(s);

	move_gap(gbuf, offset);
	while (gap_length(gbuf) <= MIN_GAP_SIZE + len)
		expand_gap(gbuf);
	while (*s)
		*(gbuf->gap_start++) = *s++;
}

// Delete n bytes after off.
void
gbf_delete(GapBuffer *gbuf, size_t offset, size_t bytes) {
	move_gap(gbuf, offset);
	if (gbuf->second_part == gbuf->end)
		return;
	gbuf->second_part += bytes;
}

// Clears the gapbuffer (deletes all text).
void
gbf_clear(GapBuffer *gbuf) {
	gbuf->gap_start = gbuf->first_part;
	gbuf->second_part = gbuf->end;
}

// Returns the byte at position off.
char
gbf_at(GapBuffer *gbuf, size_t offset) {
	if (gbuf->first_part + offset < gbuf->gap_start)
		return *(gbuf->first_part + offset);
	else
		return *(gbuf->first_part + offset + gap_length(gbuf));
}
