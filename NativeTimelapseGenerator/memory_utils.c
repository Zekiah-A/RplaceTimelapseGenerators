#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "console.h"
#include "memory_utils.h"

void init_stack(Stack* stack, size_t item_size, int max_size)
{
	stack->items = malloc(item_size * max_size);
	stack->item_size = item_size;
	stack->top = -1;
	stack->max_size = max_size;
	pthread_mutex_init(&stack->mutex, NULL);
	stack->replenished = false;
}

int push_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	if (stack->top >= stack->max_size - 1) {
		log_message(LOG_ERROR, "Stack overflow occurred\n");
		pthread_mutex_unlock(&stack->mutex);
		return 1;
	}

	stack->top++;
	memcpy((char*)stack->items + stack->top * stack->item_size, item, stack->item_size);
	stack->replenished = true;

	pthread_mutex_unlock(&stack->mutex);
    return 0;
}

int pop_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	if (stack->top < 0) {
		memset(item, 0, stack->item_size);
		pthread_mutex_unlock(&stack->mutex);
		return 1;
	}

	memcpy(item, (char*)stack->items + stack->top * stack->item_size, stack->item_size);
	stack->top--;

	pthread_mutex_unlock(&stack->mutex);
	return 0;
}

void free_stack(Stack* stack)
{
	free(stack->items);
	pthread_mutex_destroy(&stack->mutex);
}
