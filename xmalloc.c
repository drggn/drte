#include <stdio.h>
#include <stdlib.h>

void *xmalloc(size_t size){
	void *p;
	if((p = malloc(size)) != NULL){
		return p;
	}else{
		fprintf(stderr, "Can't allocate memory");
		exit(-1);
	}
}
