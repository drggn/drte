#include <stdio.h>
#include <stdlib.h>

#define NCURSES_KEYS (KEY_MAX - KEY_MIN + 1)

void *xmalloc(size_t size){
	void *p;
	if((p = malloc(size)) != NULL){
		return p;
	}else{
		fprintf(stderr, "Can't allocate memory");
		exit(-1);
	}
}

// TODO: Unicode
int
isnewline(unsigned char c){
	return c == '\n';
}

// Returns true, if c is the first byte of a white space code point.
// TODO: Unicode
int
iswhitespace(unsigned char c){
	return (c == '\t' || c == ' ' || isnewline(c));
}


// Given the first byte of a code point, the function
// returns the number of bytes the code points consist of.
size_t
bytes(unsigned char c){
	if(c < 128)
		return 1;
	else if((c & 0xF0) == 0xF0)
		return 4;
	else if((c & 0xE0) == 0xE0)
		return 3;
	else if((c & 0xC0) == 0xC0)
		return 2;
	return 0;
}
