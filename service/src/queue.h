#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct _queue queue;

/* Shared multi-process compatible queue */
queue* queue_new(char* name, unsigned long max_count, unsigned int element_size);
void queue_free(queue* ptr);

bool queue_add(queue* ptr, void* element, unsigned int len);
bool queue_remove(queue* ptr, void* element, unsigned int len);

#endif /* QUEUE_H */
