#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <avcall.h>

#include "console.h"
#include "memory_utils.h"

void init_stack(Stack* stack, size_t item_size, int default_size)
{
	stack->items = malloc(item_size * default_size);
	stack->item_size = item_size;
	stack->top = -1;
	stack->max_size = default_size;
	pthread_mutex_init(&stack->mutex, NULL);
	stack->replenished = false;
}

int push_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	// Resize if the stack is full
	if (stack->top >= stack->max_size - 1) {
		size_t new_size = stack->max_size * 2; // Double the size
		void* new_items = realloc(stack->items, new_size * stack->item_size);
		if (!new_items) {
			log_message(LOG_ERROR, "Failed to resize stack\n");
			pthread_mutex_unlock(&stack->mutex);
			return 1;
		}
		stack->items = new_items;
		stack->max_size = new_size;
	}

	// Add the item to the stack
	stack->top++;
	memcpy((char*)stack->items + stack->top * stack->item_size, item, stack->item_size);
	stack->replenished = true;

	pthread_mutex_unlock(&stack->mutex);
	return 0;
}

bool pop_stack(Stack* stack, void* item)
{
	pthread_mutex_lock(&stack->mutex);

	if (stack->top < 0) {
		memset(item, 0, stack->item_size);
		pthread_mutex_unlock(&stack->mutex);
		return false;
	}

	// Retrieve the item from the stack
	memcpy(item, (char*)stack->items + stack->top * stack->item_size, stack->item_size);
	stack->top--;

	pthread_mutex_unlock(&stack->mutex);
	return true;
}

void free_stack(Stack* stack)
{
	free(stack->items);
	pthread_mutex_destroy(&stack->mutex);
}

void init_work_queue(WorkQueue* queue, size_t capacity)
{
	queue->work = (av_alist*) malloc(sizeof(av_alist) * capacity);
	if (!queue->work) {
		stop_console();
		log_message(LOG_ERROR, "Failed to initialise main thread work queue\n");
		exit(EXIT_FAILURE);
	}

	queue->capacity = capacity;
	queue->front = 0;
	queue->rear = 0;
	queue->replenished = false;
	pthread_mutex_init(&queue->mutex, NULL);
}

// Dequeue a message
void push_work_queue(WorkQueue* queue, av_alist work)
{
	if (!queue) {
		init_work_queue(queue, DEFAULT_WORK_QUEUE_SIZE);
	}
	pthread_mutex_lock(&queue->mutex);

	// Check for queue overflow
	if ((queue->rear + 1) % queue->capacity == queue->front) {
		stop_console();
		log_message(LOG_ERROR, "Error - Work queue overflow.\n");
		pthread_mutex_unlock(&queue->mutex);
		exit(EXIT_FAILURE);
	}

	// Enqueue the message
	queue->work[queue->rear] = work;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->replenished = true;

	pthread_mutex_unlock(&queue->mutex);
}

av_alist pop_work_queue(WorkQueue* queue)
{
	pthread_mutex_lock(&queue->mutex);

	// Check underflow
	if (queue->front == queue->rear) {
		log_message(LOG_ERROR, "Error - Work queue underflow.\n");
		pthread_mutex_unlock(&queue->mutex);
		exit(EXIT_FAILURE);
	}

	// Dequeue the message
	av_alist message = queue->work[queue->front];
	queue->front = (queue->front + 1) % queue->capacity;
	if (queue->front == queue->rear) {
		queue->replenished = false;
	}

	pthread_mutex_unlock(&queue->mutex);
	return message;
}

void free_work_queue(WorkQueue* work_queue)
{
	if (!work_queue) {
		return;
	}

	free(work_queue->work);
	pthread_mutex_destroy(&work_queue->mutex);
}
