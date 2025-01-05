#pragma once
#include <stdio.h>
#include <stdlib.h>

#define AUTOFREE __attribute__((cleanup(defer_free)))
__attribute__((always_inline)) inline void defer_free(void *autofree_var) {
	void **ptr = (void **)autofree_var;
	if (*ptr) {
		free(*ptr);
		*ptr = NULL;
	}
}
