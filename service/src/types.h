#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdio.h>

typedef enum {ADD,
			  SUB,
			  BRR,
			  LT,
			  EQ,
			  DUP,
			  NEG,
			  MER,
			  NTG, /* new tag */
			  ITG, /* iterate tag */
			  GT,
			  SIL, /* set iteration level */
			  CTG, /* copy tag */
			  RTD, /* return to destination */
			  ETG, /* extract tag */
			  MUL, /* multiply */
			  XOR,
			  AND,
			  OR,
			  SHL,
			  SHR,
			  NEQ,
			  OPN,
			  RED,
			  WRT,
			  CLS,
			  GTE,
			  LTE,
			  HLT,
			  LOD,
			  LS,
              SDF, /* sendfile */
              ULK, /* unlink */
              LSK, /* lseek */
              RND, /* Random */
} opcode_type;

// defined in "types.c". Must be kept in sync with opcode_type ^
extern uint8_t opcode_to_num_inputs[];
extern opcode_type privileged_opcodes[];
#ifdef DEBUG
extern char* opcode_to_name[];
#endif

// Tags are 64 bits, split up into tag area (32 bits) and interation
// count (32 bits). 
typedef uint64_t tag_type;
typedef uint32_t tag_area_type;
typedef uint32_t iteration_count_type;
#define CREATE_TAG(tag_area, iteration_count) ((uint64_t) ((((uint64_t)tag_area) << 32) ^ iteration_count))
#define TAG_TO_TAG_AREA(t) ((uint32_t)(t >> 32))
#define TAG_TO_ITERATION_COUNT(t) ((uint32_t)(0xffffffff & t))

#define NO_TAG 0

typedef uint64_t data_type;

// destination type specifies which instruction (and which input of
// the instruction) the token should go to after execution. The first
// 28 bits are the instruction address, a left-right bit, and the last
// three bits are the matching function (used by the matching unit,
// essentially encoding information about the instruction so that the
// matching unit can do its job without fetching the instruction from
// memory).
// <instruction address (28 bits), input_1/input_2 (1 bit), matching function (3 bits)
typedef uint32_t destination_type;

// Helpful functions when working with destinations
#define DESTINATION_TO_ADDRESS(d) (d >> 4)
#define DESTINATION_TO_INPUT(d) ((d >> 3) & 0x1)
#define DESTINATION_TO_MATCHING_FUNCTION(d) (d & 0x3)
#define CREATE_DESTINATION(a,input,m) ((((a << 1) ^ input) << 3) ^ m)

#define MATCHING_ANY 2
#define MATCHING_BOTH 1
#define MATCHING_ONE 0

#define INPUT_ONE 0
#define INPUT_TWO 1

#define TRUE 1
#define FALSE 0

#define BOTH_OUTPUT_MARKER 1
#define ONE_OUTPUT_MARKER 0
typedef uint8_t marker_type;

typedef enum {NONE, ONE, TWO} instruction_literal_type;

typedef struct {
   opcode_type opcode;
   destination_type destination_1;
   destination_type destination_2;
   marker_type marker;
   data_type literal_1;
   data_type literal_2;
   instruction_literal_type instruction_literal;
} instruction;

/* 
   Destination addresses for I/O
*/
#define OUTPUTD_DESTINATION CREATE_DESTINATION(((1<<28)-1), 0, MATCHING_ONE)
#define OUTPUTS_DESTINATION CREATE_DESTINATION(((1<<28)-2), 0, MATCHING_ONE)
#define REGISTER_INPUT_HANDLER_DESTINATION CREATE_DESTINATION(((1<<28)-3), 0, MATCHING_ONE)
#define DEREGISTER_INPUT_HANDLER_DESTINATION CREATE_DESTINATION(((1<<28)-4), 0, MATCHING_ONE)
#define DEV_NULL_DESTINATION CREATE_DESTINATION(((1<<28)-5), 0, MATCHING_ONE)

typedef struct {
   data_type data_1;
   data_type data_2;
   opcode_type opcode;
   tag_type tag;   
   destination_type destination_1;
   destination_type destination_2;
   destination_type input;
   marker_type marker;
} execution_packet;


typedef struct {
   destination_type destination;
   data_type data;
   tag_type tag;
} token_type;

typedef struct {
   token_type token_1;
   token_type token_2;
} ready_token_pair_type;

typedef struct {
   token_type output_1;
   token_type output_2;
   marker_type marker;
} execution_result;

#ifdef DEBUG
void print_token(token_type token);
void print_result(execution_result result);
void print_instruction(instruction inst);
#endif


#endif /* TYPES_H */
