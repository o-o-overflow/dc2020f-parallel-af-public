#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "types.h"
#include "matching_unit.h"
#include "queue.h"

token_waiting_table_type* token_waiting_table;

key_type token_to_key(token_type token)
{
   key_type to_return;
   to_return.tag = token.tag;
   to_return.destination = DESTINATION_TO_ADDRESS(token.destination);
   return to_return;
}

unsigned long key_to_index(key_type key)
{
   uint64_t idx = key.tag + key.destination;
   return idx % token_waiting_table->table_size;
}

static inline bool key_equal(key_type key1, key_type key2)
{
   return key1.tag == key2.tag && key1.destination == key2.destination;
}

ready_token_pair_type ready_token_pair_from_tokens(token_type token_1, token_type token_2)
{
   ready_token_pair_type to_return;
   
   #ifdef DEBUG
   assert(token_1.tag == token_2.tag);
   assert(DESTINATION_TO_ADDRESS(token_1.destination) == DESTINATION_TO_ADDRESS(token_2.destination));
   assert(DESTINATION_TO_INPUT(token_1.destination) != DESTINATION_TO_INPUT(token_2.destination));
   #endif

   if (DESTINATION_TO_INPUT(token_1.destination) == INPUT_ONE)
   {
	  to_return.token_1 = token_1;
	  to_return.token_2 = token_2;
   }
   else
   {
	  to_return.token_1 = token_2;
	  to_return.token_2 = token_1;
   }
   return to_return;
}

void run_matching_unit(queue* incoming_token_queue, queue* ready_token_pair_queue, uint32_t max_table_size)
{
   token_waiting_table = (token_waiting_table_type*)malloc(sizeof(token_waiting_table_type));
   token_waiting_table->num_elements = 0;
   token_waiting_table->table_size = max_table_size;

   token_waiting_table->table = (table_elements*)malloc(sizeof(table_elements) * token_waiting_table->table_size);

   // Everything is empty
   for (int i = 0; i < token_waiting_table->table_size; i++)
   {
	  token_waiting_table->table[i].element_type = EMPTY;
   }

   while (1)
   {
	  token_type next_token;
	  ready_token_pair_type ready_token_pair;
	  queue_remove(incoming_token_queue, &next_token, sizeof(token_type));

	  // fprintf(stderr, "Matching unit got a new token\n");
	  // print_token(next_token);

	  uint8_t matching_function = DESTINATION_TO_MATCHING_FUNCTION(next_token.destination);
	  // The instruction that this is destined for only needs one
	  // input (maybe it is a monadic operator or includes a literal), so it is sent to the output.
	  // MATCHING_ANY is used for MERGE instructions, so whatever is ready is sent to the output
	  if (matching_function == MATCHING_ONE || matching_function == MATCHING_ANY)
	  {
		 ready_token_pair.token_1 = next_token;
		 queue_add(ready_token_pair_queue, &ready_token_pair, sizeof(ready_token_pair_type));		 
	  }
	  else if (matching_function == MATCHING_BOTH)
	  {
		 key_type key = token_to_key(next_token);

		 // Is the other input in our waiting table? If so, send it to the output queue (and remove the other)
		 // if the other input is not in our waiting table, then insert this token into
		 unsigned long element_index = key_to_index(key);

		 table_elements element = token_waiting_table->table[element_index];
		 if (element.element_type == EMPTY)
		 {
			add_to_waiting_table(key, next_token);
		 }
		 else
		 {
			token_type* found = NULL;
			for (table_list_type* cur = element.element; cur != NULL; cur = cur->next)
			{
			   if (key_equal(key, cur->key))
			   {
				  found = &(cur->value);
				  break;
			   }			   
			}
			if (found == NULL)
			{
			   add_to_waiting_table(key, next_token);
			}
			else
			{
			   ready_token_pair = ready_token_pair_from_tokens(next_token, *found);
			   queue_add(ready_token_pair_queue, &ready_token_pair, sizeof(ready_token_pair_type));
			   remove_from_waiting_table(key);
			}
		 }
	  }
	  else
	  {
		 #ifdef DEBUG
		 fprintf(stderr, "ERROR: unhandled matching function %d\n", matching_function);
		 assert(false);
		 #endif
	  }
   }
}

void add_to_waiting_table(key_type key, token_type token)
{
   unsigned long element_index = key_to_index(key);
   table_elements* element = &token_waiting_table->table[element_index];
   table_list_type* next = NULL;
   
   if (element->element_type == EMPTY)
   {
	  element->element_type = LIST;
   }
   else
   {
	  next = element->element;
   }

   element->element = (table_list_type*) malloc(sizeof(table_list_type));
   element->element->key = key;
   element->element->value = token;
   element->element->next = next;
}

/* Required that key exists in the waiting table */
void remove_from_waiting_table(key_type key)
{
   unsigned long element_index = key_to_index(key);
   table_elements* element = &token_waiting_table->table[element_index];

   #ifdef DEBUG
   assert(element->element_type == LIST);
   #endif


   if (key_equal(element->element->key, key))
   {
	  if (element->element->next == NULL)
	  {
		 element->element_type = EMPTY;
		 free(element->element);
	  }
	  else
	  {
		 table_list_type* to_free = element->element;
		 
		 element->element = element->element->next;
		 free(to_free);
	  }
   }
   else
   {
	  bool found = false;
	  table_list_type* prev = element->element;
	  for (table_list_type* cur = element->element->next; cur != NULL; cur = cur->next)
	  {
		 if (key_equal(cur->key, key))
		 {
			prev->next = cur->next;
			free(cur);
			found = true;
			break;
		 }
		 prev = cur;
	  }
	  #ifdef DEBUG
	  assert(found);
	  #endif
   }
}
