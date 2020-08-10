#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "instruction_store.h"

instruction* instructions = NULL;
uint32_t num_instructions = 0;

export_node* exports = NULL;

export_node* find_export(char* name)
{
   export_node* cur = exports;
   while (cur != NULL)
   {
	  if (strcmp(cur->name, name) == 0)
	  {
		 return cur;
	  }
	  cur = cur->next;
   }
   return NULL;
}

void add_ready_instructions(instruction* instructions, uint32_t num_instructions, queue* executable_packet_queue)
{
   for (int i = 0; i < num_instructions; i++)
   {
	  instruction inst = instructions[i];
	  if (inst.instruction_literal == TWO)
	  {
		 execution_packet ready = {
			.data_1 = inst.literal_1,
			.data_2 = inst.literal_2,
			.opcode = inst.opcode,
			.tag = NO_TAG,
			.destination_1 = inst.destination_1,
			.destination_2 = inst.destination_2,
            .input = CREATE_DESTINATION(i, 0, 0),
			.marker = inst.marker,
		 };
		 queue_add(executable_packet_queue, &ready, sizeof(execution_packet));
	  }
	  else if (inst.instruction_literal == ONE &&
			   opcode_to_num_inputs[inst.opcode] == 1)
	  {
		 execution_packet ready = {
			.data_1 = inst.literal_1,
			.data_2 = 0,
			.opcode = inst.opcode,
			.tag = NO_TAG,
			.destination_1 = inst.destination_1,
			.destination_2 = inst.destination_2,
            .input = CREATE_DESTINATION(i, 0, 0),
			.marker = inst.marker,
		 };
		 queue_add(executable_packet_queue, &ready, sizeof(execution_packet));
	  }
   }
}

bool is_in_list(destination_to_update* constants, uint32_t num_constant, uint32_t cur, int which_destination)
{
   for (int i = 0; i < num_constant; i++)
   {
	  if (constants[i].instruction_number == cur)
	  {
		 if (which_destination == FIRST_DESTINATION && (constants[i].flags.is_first_destination))
		 {
			return true;
		 }
		 else if (which_destination == SECOND_DESTINATION && (constants[i].flags.is_second_destination))
		 {
			return true;
		 }
		 else if (which_destination == FIRST_LITERAL && (constants[i].flags.is_first_literal))
		 {
			return true;
		 }
		 else if (which_destination == SECOND_LITERAL && (constants[i].flags.is_second_literal))
		 {
			return true;
		 }
	  }
   }
   return false;
}

int read_all(int fd, char* buf, int buf_size)
{
   int total_read = 0;
   int n_read;
   while((n_read = read(fd, buf+total_read, buf_size - total_read)) > 0)
   {
	  total_read += n_read;
   }
   return ((n_read < 0) ? n_read : total_read);
}

destination_type static inline increment_destination_address(destination_type destination, uint32_t to_add)
{
   destination_type addr = DESTINATION_TO_ADDRESS(destination);
   addr += to_add;
   return CREATE_DESTINATION(addr,
							 DESTINATION_TO_INPUT(destination),
							 DESTINATION_TO_MATCHING_FUNCTION(destination));
}

loaded_code_info load_file(int fd, off_t file_size, bool is_privileged)
{
   loaded_code_info to_return = { .instructions = NULL,
								  .current_num_instructions = 0,
								  .exports = NULL,
								  .error = 0
   };
   if (file_size < sizeof(sephi_header))
   {
	  #ifdef DEBUG
	  fprintf(stderr, "Error: load_file file does not have enough size even for a header: %d\n", file_size);
	  #endif
	  to_return.error = -1;
	  return to_return;
   }

   // Read in the file
   char* content = malloc(file_size);
   if (content == NULL)
   {
	  #ifdef DEBUG
	  perror("Error: malloc failed");
	  #endif
	  to_return.error = -1;
	  return to_return;
   }

   char* end = content + file_size;

   int result = read_all(fd, content, file_size);
   if (result < 0)
   {
	  #ifdef DEBUG
	  fprintf(stderr, "Error: Unable to read all of the file\n");
	  #endif
	  goto fail;
   }

   sephi_header* header = (sephi_header*)content;

   if (memcmp(header->magic_bytes, MAGIC_BYTES, 8) != 0)
   {
	  #ifdef DEBUG
	  fprintf(stderr, "Error: file does not have the right magic bytes\n");
	  #endif
	  goto fail;
   }

   destination_to_update* constants = (destination_to_update*)(content + sizeof(sephi_header));
   uint32_t num_constant = header->num_constant;

   destination_to_update* to_fix = (destination_to_update*)(constants + num_constant);
   uint32_t num_to_fix = header->num_to_fix;

   external_reference* external_references = (external_reference*)(to_fix + num_to_fix);
   uint32_t num_external_references = header->num_external_ref;

   export_symbol* this_exports = (export_symbol*)(external_references + num_external_references);
   uint32_t num_exports = header->num_exported;

   instruction* start_instructions = (instruction*)(this_exports + num_exports);
   int64_t header_size = (char*)start_instructions - (char*)header;
   int64_t instruction_size = file_size - header_size;

   #ifdef DEBUG
   fprintf(stderr, "header: %p\nend: %p\nconstants: %p\nnum_constant: %d\nto_fix: %p\nnum_to_fix: %d\nexternal_references: %p\nnum_external_references: %d\nexports: %p\nnum_exports: %d\nheader_size: %d\nstart_instructions: %p\ninstruction_size: %d\n",
		   header,
		   end,
		   constants,
		   num_constant,
		   to_fix,
		   num_to_fix,
		   external_references,
		   num_external_references,
		   this_exports,
		   num_exports,
		   header_size,
		   start_instructions,
		   instruction_size);
   #endif

   // Sanity checking
   if (constants > end ||
	   to_fix > end ||
	   external_references > end ||
	   this_exports > end ||
	   start_instructions > end)
   {
	  #ifdef DEBUG
	  fprintf(stderr, "Error: file has messed up headers\n");
	  #endif
	  goto fail;
   }

   if (instruction_size % sizeof(instruction) != 0)
   {
	  #ifdef DEBUG
	  fprintf(stderr, "Error: file is not a multiple of instruction size\n");
	  #endif
	  goto fail;
   }

   int current_num_instructions = instruction_size / sizeof(instruction);

   for (uint32_t i = 0; i < num_external_references; i++)
   {
	  external_references[i].name[REFERENCE_MAX_SIZE-1] = '\0';
	  export_node* result = find_export(external_references[i].name);
	  // check: do we have symbols for all the externed symbols?
	  if (result == NULL)
	  {
		 #ifdef DEBUG
		 fprintf(stderr, "Error: could not find external reference %s\n", external_references[i].name);
		 #endif
		 goto fail;
	  }
   }

   int start_instruction_num = num_instructions;
   instructions = (instruction*)realloc(instructions, instruction_size + (num_instructions * sizeof(instruction)));

   num_instructions += current_num_instructions;

   for (uint32_t i = 0; i < current_num_instructions; i++)
   {
	  // update destinations unless it's in the constant tag. 
	  instruction* inst = start_instructions+i;
	  if (!is_in_list(constants, num_constant, i, FIRST_DESTINATION))
	  {
		 if (inst->destination_1 != DEV_NULL_DESTINATION)
		 {
			inst->destination_1 = increment_destination_address(inst->destination_1, start_instruction_num);
		 }
	  }
	  if (!is_in_list(constants, num_constant, i, SECOND_DESTINATION))
	  {
		 if (inst->destination_2 != DEV_NULL_DESTINATION)
		 {
			inst->destination_2 = increment_destination_address(inst->destination_2, start_instruction_num);
		 }
	  }
	  // Check about the to_fix ones
	  if(is_in_list(to_fix, num_to_fix, i, FIRST_LITERAL))
	  {
		 inst->literal_1 = increment_destination_address(inst->literal_1, start_instruction_num);
	  }
	  if(is_in_list(to_fix, num_to_fix, i, SECOND_LITERAL))
	  {
		 inst->literal_2 = increment_destination_address(inst->literal_2, start_instruction_num);
	  }

	  // user-space code cannot call these fun instructions
	  if (!is_privileged)
	  {
		 if (inst->opcode == OPN ||
			 inst->opcode == RED ||
			 inst->opcode == WRT ||
			 inst->opcode == CLS ||
			 inst->opcode == HLT ||
			 inst->opcode == LOD ||
			 inst->opcode == LS ||
             inst->opcode == SDF ||
             inst->opcode == ULK ||
             inst->opcode == LSK ||
             inst->opcode == RND
            )
		 {
			inst->opcode = DUP;
		 }
	  }
	  memcpy(instructions+start_instruction_num+i, start_instructions+i, sizeof(instruction));
   }

   for (uint32_t i = 0; i < num_external_references; i++)
   {
	  export_node* result = find_export(external_references[i].name);

	  // Now, resolve the external references.
	  destination_to_update destination = external_references[i].destination;
	  uint32_t inst_num = destination.instruction_number;

	  if (inst_num > current_num_instructions)
	  {
		 #ifdef DEBUG
		 fprintf(stderr, "Error: external reference %s was out of bounds %d\n", external_references[i].name, inst_num);
		 #endif
		 goto fail;
	  }
	  instruction* inst = instructions+start_instruction_num+inst_num;
	  if (destination.flags.is_first_destination)
	  {
		 inst->destination_1 = result->current_destination;
	  }
	  if (destination.flags.is_second_destination)
	  {
		 inst->destination_2 = result->current_destination;
	  }
	  if (destination.flags.is_first_literal)
	  {
		 inst->literal_1 = result->current_destination;
	  }
	  if (destination.flags.is_second_literal)
	  {
		 inst->literal_2 = result->current_destination;
	  }
      // BUG: undocumented functionality to change the opcode
      if ((destination.flags.raw & 0x80) != 0)
      {
         inst->opcode = result->current_destination;
      }
   }

   // Update our store with the exports.

   export_node* tail = exports;
   export_node* first_export = NULL;
   while ((tail != NULL) && (tail->next != NULL))
   {
	  tail = tail->next;
   }

   for (uint32_t i = 0; i < num_exports; i++)
   {
	  export_symbol* export = this_exports+i;
	  export->name[REFERENCE_MAX_SIZE-1] = '\0';
	  export_node* new_export_node = (export_node*) malloc(sizeof(export_node));
	  new_export_node->current_destination = increment_destination_address(export->local_destination, start_instruction_num);
	  strcpy(new_export_node->name, export->name);
	  new_export_node->next = NULL;
	  if (first_export == NULL)
	  {
		 first_export = new_export_node;
	  }
	  if (tail == NULL)
	  {
		 tail = new_export_node;
		 exports = tail;
	  }
	  else
	  {
		 tail->next = new_export_node;
		 tail = new_export_node;
	  }
   }

   free(content);
   to_return.instructions = instructions+start_instruction_num;
   to_return.current_num_instructions = current_num_instructions;
   to_return.exports = first_export;
   to_return.error = 0;
   return to_return;

  fail:
   free(content);
   to_return.error = -1;
   return to_return;

}

void run_instruction_store(char* os_filename, queue* ready_token_pair_queue, queue* executable_packet_queue)
{
   #ifdef DEBUG
   fprintf(stderr, "sephi_header: %ld\n", sizeof(sephi_header));
   fprintf(stderr, "flags: %ld\n", sizeof(flags));
   fprintf(stderr, "destination_to_update: %ld\n", sizeof(destination_to_update));
   fprintf(stderr, "external_reference: %ld\n", sizeof(external_reference));
   fprintf(stderr, "export_symbol: %ld\n", sizeof(export_symbol));
   #endif

   int fd = open(os_filename, O_RDONLY);
   if (fd == -1)
   {
	  #ifdef DEBUG
	  perror("open failed");
	  #endif
	  return;
   }
   off_t file_size = lseek(fd, 0, SEEK_END);
   lseek(fd, 0, SEEK_SET);

   #ifdef DEBUG
   fprintf(stderr, "%s instruction filesize %ld\n", os_filename, file_size);
   #endif

   loaded_code_info result = load_file(fd, file_size, true);
   close(fd);
   if (result.error == -1)
   {
      #ifdef DEBUG
	  fprintf(stderr, "load_file returned error\n");
      #endif
	  exit(-1);
   }
   
   // when we start up, go through all the initial instructions and
   // make any that have two literal instructions (or one for monadic
   // functions) ready.
   add_ready_instructions(instructions, num_instructions, executable_packet_queue);

   // now get ready token pairs and do stuff
   while (1)
   {
	  ready_token_pair_type next;
	  queue_remove(ready_token_pair_queue, &next, sizeof(ready_token_pair_type));

	  uint32_t address = DESTINATION_TO_ADDRESS(next.token_1.destination);

	  if (address >= num_instructions)
	  {
		 #ifdef DEBUG
		 fprintf(stderr, "ERROR, destination address %d is out of bounds on the number of instructions %d. Ignoring", address, num_instructions);
		 print_token(next.token_1);
		 print_token(next.token_2);
		 #endif
		 continue;
	  }

	  instruction inst = instructions[address];
	  uint8_t num_inputs = opcode_to_num_inputs[inst.opcode];

	  if (inst.opcode == LOD)
	  {
		 // We need to load some code! We handle this instruction.

		 char* filename = (char*)&next.token_1.data;
		 filename[7] = '\0';
		 data_type arg = next.token_2.data;

		 int fd = open(filename, O_RDONLY);
		 if (fd == -1)
		 {
			#ifdef DEBUG
			perror("open failed");
			#endif
			goto load_fail;
		 }
		 off_t file_size = lseek(fd, 0, SEEK_END);
		 lseek(fd, 0, SEEK_SET);

		 #ifdef DEBUG
		 fprintf(stderr, "%s instruction filesize %ld\n", filename, file_size);
		 #endif

		 loaded_code_info result = load_file(fd, file_size, false);
		 close(fd);
		 if (result.error == -1)
		 {
			#ifdef DEBUG
			fprintf(stderr, "load_file returned error\n");
			#endif

			goto load_fail;
		 }
		 {
			// call the main function using the exports.
			char* main_arg_name = "_main_arg_0_export";
			char* main_return_location_name = "_main_return_location_export";
			export_node* main_arg = NULL;
			export_node* main_return_location = NULL;

			export_node* cur = result.exports;
			while(cur != NULL)
			{
			   if (strcmp(cur->name, main_arg_name) == 0)
			   {
				  main_arg = cur;
			   }
			   if (strcmp(cur->name, main_return_location_name) == 0)
			   {
				  main_return_location = cur;
			   }
			   cur = cur->next;
			}

			if (main_arg != NULL && main_return_location != NULL)
			{
			   // we good, let's add some fake packets to make it seem like we're calling this function
			   execution_packet arg_packet = {
				  .data_1 = arg,
				  .data_2 = 0,
				  .opcode = DUP,
				  .tag = next.token_1.tag,
				  .destination_1 = main_arg->current_destination,
				  .destination_2 = DEV_NULL_DESTINATION,
                  .input = CREATE_DESTINATION(address, 0, 0),
				  .marker = ONE_OUTPUT_MARKER,
			   };
			   queue_add(executable_packet_queue, &arg_packet, sizeof(execution_packet));

			   // now add the return value
			   execution_packet return_loc = {
				  .data_1 = inst.destination_1,
				  .data_2 = 0,
				  .opcode = DUP,
				  .tag = next.token_1.tag,
				  .destination_1 = main_return_location->current_destination,
				  .destination_2 = DEV_NULL_DESTINATION,
                  .input = CREATE_DESTINATION(address, 0, 0),
				  .marker = ONE_OUTPUT_MARKER,
			   };
			   queue_add(executable_packet_queue, &return_loc, sizeof(execution_packet));
			}

			add_ready_instructions(result.instructions, result.current_num_instructions, executable_packet_queue);
			continue;

		   load_fail:
			{
			   // Change the packet so that the result of the load is a -1
			   execution_packet error_packet = {
				  .data_1 = -1,
				  .data_2 = 0,
				  .opcode = DUP,
				  .tag = next.token_1.tag,
				  .destination_1 = inst.destination_1,
				  .destination_2 = inst.destination_2,
                  .input = CREATE_DESTINATION(address, 0, 0),
				  .marker = inst.marker,
			   };
			   queue_add(executable_packet_queue, &error_packet, sizeof(execution_packet));
			   continue;
			}

		 }
		 continue;
	  }
	  
	  if (inst.instruction_literal == NONE && num_inputs == 2)
	  {
		 #ifdef DEBUG
         if (DESTINATION_TO_ADDRESS(next.token_1.destination) != DESTINATION_TO_ADDRESS(next.token_2.destination))
         {
            printf("oops %d\n", address);
            print_instruction(inst);
            print_token(next.token_1);
            print_token(next.token_2);
         }
		 assert(DESTINATION_TO_ADDRESS(next.token_1.destination) == DESTINATION_TO_ADDRESS(next.token_2.destination));
		 assert(DESTINATION_TO_INPUT(next.token_1.destination) == INPUT_ONE);
		 assert(DESTINATION_TO_INPUT(next.token_2.destination) == INPUT_TWO);
		 assert(next.token_1.tag == next.token_2.tag);
		 #endif
		 
		 execution_packet ready = {
			.data_1 = next.token_1.data,
			.data_2 = next.token_2.data,
			.opcode = inst.opcode,
			.tag = next.token_1.tag,
			.destination_1 = inst.destination_1,
			.destination_2 = inst.destination_2,
            .input = CREATE_DESTINATION(address, 0, 0),
			.marker = inst.marker,
		 };
		 queue_add(executable_packet_queue, &ready, sizeof(execution_packet));
	  }
	  else if (inst.instruction_literal == ONE || num_inputs == 1)
	  {
		 execution_packet ready = {
			.data_1 = next.token_1.data,
			.data_2 = inst.literal_1,
			.opcode = inst.opcode,
			.tag = next.token_1.tag,
			.destination_1 = inst.destination_1,
			.destination_2 = inst.destination_2,
            .input = CREATE_DESTINATION(address, 0, 0),
			.marker = inst.marker,
		 };
		 queue_add(executable_packet_queue, &ready, sizeof(execution_packet));
	  }
	  else
	  {
		 // should never get here, because instructions that have two
		 // literal inputs are already sent to be executed when
		 // instructions are added.
		 #ifdef DEBUG
		 assert(false);
		 #endif 
	  }
	  
   }
}
