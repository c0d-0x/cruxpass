#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../include/tui.h"

bool enqueue(queue_t *queue, int64_t index) {
  if (queue_is_empty(queue)) {
    if ((queue->data = calloc(QUEUE_MAX, sizeof(int64_t))) == NULL) {
      return false;
    }

    queue->capacity = QUEUE_MAX;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
  }

  if (queue_is_full(queue)) {
    int64_t *new_data = realloc(queue->data, sizeof(int64_t) * queue->capacity * 2);
    if (new_data == NULL) {
      return false;
    }

    queue->capacity *= 2;
    queue->data = new_data;
    queue->tail = queue->size;
  }

  queue->data[queue->tail] = index;
  queue->tail = (queue->tail + 1) % queue->capacity;
  queue->size++;

  return true;
}

int64_t dequeue(queue_t *queue) {
  if (queue_is_empty(queue)) {
    return -2;
  }

  int64_t index = queue->data[queue->head];
  queue->head = (queue->head + 1) % queue->size;
  return index;
}

bool queue_is_full(queue_t *queue) { return queue->capacity == queue->size; }

bool queue_is_empty(queue_t *queue) { return queue->data == NULL; }

void free_queue(queue_t *queue) {
  if (queue->data != NULL) {
    free(queue->data);
    queue->data = NULL;
    queue->capacity = 0;
    queue->head = 0;
    queue->size = 0;
    queue->tail = 0;
  }
}
