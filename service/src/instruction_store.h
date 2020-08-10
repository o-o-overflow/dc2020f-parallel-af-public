#ifndef INSTRUCTION_STORE_H
#define INSTRUCTION_STORE_H

#include "types.h"
#include "queue.h"
#include "sephi.h"

#define FIRST_DESTINATION 1
#define SECOND_DESTINATION 2
#define FIRST_LITERAL 3
#define SECOND_LITERAL 4

typedef struct _current_export_node {
   destination_type current_destination;
   char name[REFERENCE_MAX_SIZE];
   struct _current_export_node* next;
} export_node;

typedef struct {
   instruction* instructions;
   int current_num_instructions;
   export_node* exports;
   int error;
} loaded_code_info;

void run_instruction_store(char* os_filename, queue* ready_token_pair_queue, queue* executable_packet_queue);

#endif /* INSTRUCTION_STORE_H */
