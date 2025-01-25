#pragma once
#include <avcall.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define arrclear(a) ((a) ? stbds_header(a)->length = 0 : 0)

#define NOSANITIZE __attribute__((no_sanitize("address", "undefined")))

#define AUTOFREE __attribute__((cleanup(defer_free)))
__attribute__((always_inline)) inline void defer_free(void *autofree_var) {
	void **ptr = (void **)autofree_var;
	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
}

#define DEFAULT_STACK_SIZE 256

typedef struct stack {
	void* items;
	size_t item_size;
	long top;
	ssize_t max_size;
	pthread_mutex_t mutex;
	bool replenished;
} Stack;
void init_stack(Stack* stack, size_t item_size, ssize_t max_size);
int push_stack(Stack* stack, void* item);
bool pop_stack(Stack* stack, void* item);
void free_stack(Stack* stack);

#define DEFAULT_WORK_QUEUE_SIZE 64

typedef struct work_queue {
	av_alist* work;
	size_t capacity;
	size_t front;
	size_t rear;
	bool replenished;
	pthread_mutex_t mutex;
} WorkQueue;
void init_work_queue(WorkQueue* queue, size_t capacity);
void push_work_queue(WorkQueue* queue, av_alist work);
av_alist pop_work_queue(WorkQueue* queue);
void free_work_queue(WorkQueue* work_queue);