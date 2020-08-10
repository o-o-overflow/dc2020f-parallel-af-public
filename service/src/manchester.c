#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "input_module.h"
#include "instruction_store.h"
#include "io_switch.h"
#include "manchester.h"
#include "matching_unit.h"
#include "processing_unit.h"
#include "queue.h"

#define SIZE_MATCHING_STORE 2048
#define MAX_QUEUE_SIZE 1024

#ifdef DEBUG
void print_token(token_type token)
{
   fprintf(stderr, "Token dest_addr=%d dest_input=%d data=%lu tag=0x%lx\n", DESTINATION_TO_ADDRESS(token.destination), DESTINATION_TO_INPUT(token.destination), token.data, token.tag);
}
#endif

#ifdef DEBUG
void print_result(execution_result result)
{
   print_token(result.output_1);
   if (result.marker == BOTH_OUTPUT_MARKER)
   {
	  print_token(result.output_2);
   }
}

void print_instruction(instruction inst)
{
   fprintf(stderr, "opcode=%s dest_1=%d dest_2=%d",
           opcode_to_name[inst.opcode],
           inst.destination_1,
           inst.destination_2);
}

#endif

void start_machine(char* os_filename, int timeout)
{
   queue* execution_token_output_queue;
   queue* matching_unit_input_queue;
   queue* ready_token_pair_queue;
   queue* preprocessed_executable_packet_queue;
   queue* processed_executable_packet_queue;

   #ifdef DEBUG
   char* execution_token_output_queue_name = "/manchester-token-output";
   #else
   char* execution_token_output_queue_name = "/1";
   #endif

   #ifdef DEBUG
   char* matching_unit_input_queue_name = "/manchester-token-input";
   #else
   char* matching_unit_input_queue_name = "/2";
   #endif

   #ifdef DEBUG
   char* ready_token_pair_queue_name = "/manchester-ready-token-pair";
   #else
   char* ready_token_pair_queue_name = "/3";
   #endif

   #ifdef DEBUG
   char* preprocessed_executable_packet_queue_name = "/manchester-pexecutable-packet";
   #else
   char* preprocessed_executable_packet_queue_name = "/4";
   #endif

   #ifdef DEBUG
   char* processed_executable_packet_queue_name = "/manchester-executable-packets";
   #else
   char* processed_executable_packet_queue_name = "/5";
   #endif
   
   execution_token_output_queue = queue_new(execution_token_output_queue_name, MAX_QUEUE_SIZE, sizeof(token_type));
   matching_unit_input_queue = queue_new(matching_unit_input_queue_name, MAX_QUEUE_SIZE, sizeof(token_type));
   ready_token_pair_queue = queue_new(ready_token_pair_queue_name, MAX_QUEUE_SIZE, sizeof(ready_token_pair_type));
   preprocessed_executable_packet_queue = queue_new(preprocessed_executable_packet_queue_name, MAX_QUEUE_SIZE, sizeof(execution_packet));
   processed_executable_packet_queue = queue_new(processed_executable_packet_queue_name, MAX_QUEUE_SIZE, sizeof(execution_packet));

   pid_t instruction_store = fork();
   if (instruction_store == 0)
   {
	  run_instruction_store(os_filename, ready_token_pair_queue, preprocessed_executable_packet_queue);
   }

   pid_t input_module = fork();
   if (input_module == 0)
   {
	  run_input_module(preprocessed_executable_packet_queue, processed_executable_packet_queue);
   }

   pid_t processing_unit = fork();
   if (processing_unit == 0)
   {
	  run_processing_unit(processed_executable_packet_queue, execution_token_output_queue);
   }

   pid_t io_switch = fork();
   if (io_switch == 0)
   {
	  run_io_switch(execution_token_output_queue, matching_unit_input_queue);
   }

   pid_t matching_unit = fork();
   if (matching_unit == 0)
   {
	  run_matching_unit(matching_unit_input_queue, ready_token_pair_queue, SIZE_MATCHING_STORE);
   }

   pid_t timeout_process = fork();
   if (timeout_process == 0)
   {
	  sleep(timeout);
	  exit(0);
   }

   // wait for any of the children to die.
   wait(NULL);

   kill(processing_unit, 9);
   kill(instruction_store, 9);
   kill(matching_unit, 9);
   kill(io_switch, 9);
   kill(input_module, 9);
   kill(timeout_process, 9);
}

int main(int argc, char** argv)
{
   setvbuf(stdout, NULL, _IONBF, 0);
   #ifdef DEBUG
   fprintf(stderr, "instruction: %ld\n", sizeof(instruction));
   fprintf(stderr, "opcode_type: %ld\n", sizeof(opcode_type));
   fprintf(stderr, "destination_type: %ld\n", sizeof(destination_type));
   fprintf(stderr, "marker_type: %ld\n", sizeof(marker_type));
   fprintf(stderr, "data_type: %ld\n", sizeof(data_type));
   fprintf(stderr, "instruction_literal_type: %ld\n", sizeof(instruction_literal_type));

   fprintf(stderr, "\noffsets\n");
   fprintf(stderr, "offsets:\nopcode: %zd\ndestination_1: %zd\ndestination_2: %zd\nmarker: %zd\nliteral_1: %zd\nliteral_2: %zd\ninstruction_literal: %zd\n",
		  offsetof(instruction, opcode),
		  offsetof(instruction, destination_1),
		  offsetof(instruction, destination_2),
		  offsetof(instruction, marker),
		  offsetof(instruction, literal_1),
		  offsetof(instruction, literal_2),
		  offsetof(instruction, instruction_literal));


   fprintf(stderr, "O_WRONLY: %p\nO_CREAT: %p\nO_TRUNC: %p\nO_RDONLY: %p\n", O_WRONLY, O_CREAT, O_TRUNC, O_RDONLY);

   #endif
   int opt;
   char* filename = NULL;
   int timeout = 5;

   while ((opt = getopt(argc, argv, "f:t:")) != -1)
   {
	  switch (opt) {
		 case 'f':
			filename = strdup(optarg);
			break;

		 case 't':
			timeout = atoi(optarg);
			break;

		 default:
			fprintf(stderr, "Usage: %s [-f initial_program] [-t timeout]\n", argv[0]);
			exit(-1);
	  }
   }

   if (filename == NULL)
   {
	  fprintf(stderr, "Error, must specify an initial filename.\n");
	  exit(-1);
   }

   start_machine(filename, timeout);

   return 0;
}

