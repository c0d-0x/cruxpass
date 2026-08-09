#include "tui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

bool enqueue(queue_t *queue, int64_t index) {
    if (queue_empty(queue)) {
        if ((queue->data = calloc(QUEUE_MAX, sizeof(int64_t))) == NULL) return false;
        queue->capacity = QUEUE_MAX;
    }

    if (queue_full(queue)) {
        int64_t *new_data = realloc(queue->data, sizeof(int64_t) * queue->capacity * 2);
        if (new_data == NULL) return false;
        queue->capacity *= 2;
        queue->data = new_data;
        queue->tail = queue->count;
    }

    queue->data[queue->tail] = index;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    return true;
}

int64_t dequeue(queue_t *queue) {
    if (queue_empty(queue)) return QUEUE_ERR;

    int64_t index = queue->data[queue->head];
    queue->head = (queue->head + 1) % queue->count;
    return index;
}

bool queue_index_in(queue_t *queue, int index) {
    for (int i = 0; !queue_empty(queue) && i < queue->count; i++)
        if (dequeue(queue) == index) return true;

    return false;
}

bool queue_full(queue_t *queue) { return queue != NULL && queue->capacity == queue->count && queue->data != NULL; }

bool queue_empty(queue_t *queue) { return queue == NULL || queue->data == NULL; }

void queue_free(queue_t *queue) {
    if (queue->data != NULL) {
        free(queue->data);
        queue->data = NULL;
        queue->capacity = 0;
        queue->head = 0;
        queue->count = 0;
        queue->tail = 0;
    }
}
