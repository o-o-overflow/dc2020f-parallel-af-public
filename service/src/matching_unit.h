#ifndef MATCHING_UNIT_H
#define MATCHING_UNIT_H

#include "types.h"
#include "queue.h"

typedef struct {
   tag_type tag;
   destination_type destination;
} key_type;

typedef struct _table_list {
   key_type key;
   token_type value;
   struct _table_list* next;
} table_list_type;

typedef enum {EMPTY, LIST} table_element_type;

typedef struct {
   table_element_type element_type;
   table_list_type* element;
} table_elements;

typedef struct {
   unsigned long num_elements;
   unsigned long table_size;
   table_elements* table;
} token_waiting_table_type;

void run_matching_unit(queue* incoming_token_queue, queue* ready_token_pair_queue, uint32_t max_table_size);
void add_to_waiting_table(key_type key, token_type token);
void remove_from_waiting_table(key_type key);

#endif /* MATCHING_UNIT_H */
