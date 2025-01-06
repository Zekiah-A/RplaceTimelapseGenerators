#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define AUTOFREE __attribute__((cleanup(defer_free)))
__attribute__((always_inline)) inline void defer_free(void *autofree_var) {
	void **ptr = (void **)autofree_var;
	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
}

typedef struct stack {
	void* items;
	size_t item_size;
	int top;
	int max_size;
	pthread_mutex_t mutex;
	bool replenished;
} Stack;
void init_stack(Stack* stack, size_t item_size, int max_size);
int push_stack(Stack* stack, void* item);
int pop_stack(Stack* stack, void* item);
void free_stack(Stack* stack);