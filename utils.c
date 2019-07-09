#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils.h"

// Returns true, if s ant t are equal.
bool
streql(char *s, char *t) {
	return strcmp(s, t) == 0;
}


// Like malloc, but exits on OOM.
void *
xmalloc(size_t size) {
	void *p;
	if ((p = malloc(size)) != NULL) {
		return p;
	} else {
		fprintf(stderr, "Can't allocate memory");
		exit(-1);
	}
}

// TODO: Unicode
int
is_newline(unsigned char c) {
	return c == '\n';
}

// Returns true, if c is the first byte of a white space code point.
// TODO: Unicode
int
is_whitespace(unsigned char c) {
	return (c == '\t' || c == ' ' || is_newline(c));
}


// Given the first byte of a code point, the function
// returns the number of bytes the code points consist of.
size_t
bytes(unsigned char c) {
	if (c < 128)
		return 1;
	else if ((c & 0xF0) == 0xF0)
		return 4;
	else if ((c & 0xE0) == 0xE0)
		return 3;
	else if ((c & 0xC0) == 0xC0)
		return 2;
	return 0;
}
