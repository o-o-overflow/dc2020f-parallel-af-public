#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "processing_unit.h"
#include "queue.h"
#include "khash.h"

#define TRAP_CODE_LIMIT 100

#ifdef ENABLE_TRAP_MODE
bool trap_flag = true;
#else
bool trap_flag = false;
#endif



KHASH_MAP_INIT_INT64(trap_waiting, token_type)

uint64_t hash(uint64_t var)
{
   return kh_int64_hash_func(var);
}

tag_area_type new_tag_area()
{
   return (tag_area_type) random();
}


void send_result(execution_result result, queue* outgoing_token_packets)
{   
   // Always add the first output
   queue_add(outgoing_token_packets, &result.output_1, sizeof(token_type));

   if (result.marker == BOTH_OUTPUT_MARKER)
   {
      queue_add(outgoing_token_packets, &result.output_2, sizeof(token_type));
   }
}

void single_step_token(destination_type input, token_type token, queue* outgoing_token_packets, khash_t(trap_waiting) *hash_table)
{
   tag_area_type new_tag = new_tag_area();
   token_type destination_0 = {
      .destination = CREATE_DESTINATION(0, INPUT_ONE, MATCHING_ONE),
      .data = hash(input),
      .tag = new_tag,
   };
   queue_add(outgoing_token_packets, &destination_0, sizeof(token_type));

   #ifdef DEBUG
   print_token(destination_0);
   #endif

   token_type destination_1 = {
      .destination = CREATE_DESTINATION(1, INPUT_ONE, MATCHING_ONE),
      .data = hash(token.destination),
      .tag = new_tag,
   };
   queue_add(outgoing_token_packets, &destination_1, sizeof(token_type));

   #ifdef DEBUG
   print_token(destination_1);
   #endif

   destination_type random_dest = (destination_type) random();

   token_type destination_2 = {
      .destination = CREATE_DESTINATION(2, INPUT_ONE, MATCHING_ONE),
      .data = random_dest,
      .tag = new_tag,
   };
   queue_add(outgoing_token_packets, &destination_2, sizeof(token_type));

   #ifdef DEBUG
   print_token(destination_2);
   #endif

   // TODO: store new_tag, random_dest as the keys in a hash table that also has the token.

   int ret;
   khint_t k = kh_put(trap_waiting, hash_table, random_dest, &ret);
   kh_value(hash_table, k) = token;

   /* #ifdef DEBUG */
   /* fprintf(stderr, " result of %p\n", */
   /*         next.input); */

   /* #endif */

}

void run_processing_unit(queue* incoming_execution_packets, queue* outgoing_token_packets)
{
   khash_t(trap_waiting) *hash_table = kh_init(trap_waiting);
   
   pid_t pid = getpid();
   while(1)
   {
	  execution_packet next;
	  execution_result result;
	  queue_remove(incoming_execution_packets, &next, sizeof(execution_packet));

	  result = function_unit(next);

      if (trap_flag && next.opcode == RTD)
      {
         khint_t k = kh_get(trap_waiting, hash_table, result.output_1.destination);
         if (k != kh_end(hash_table))
         {

            #ifdef DEBUG
            fprintf(stderr, "Result of trap %p, %d\n",
                    DESTINATION_TO_ADDRESS(next.input),
                    result.output_1.destination);
            print_result(result);
            #endif

            // This packet is the result of the single step
            // Check the result, if it's good then we send the original token
            if (result.output_1.data == 1)
            {
               token_type to_send = kh_value(hash_table, k);
               #ifdef DEBUG
               print_token(to_send);
               #endif
               queue_add(outgoing_token_packets, &to_send, sizeof(token_type));
            }
            kh_del(trap_waiting, hash_table, k);
            continue;
         }
      }

      if (trap_flag && ((uint16_t)DESTINATION_TO_ADDRESS(next.input) >= TRAP_CODE_LIMIT))
      {

         #ifdef DEBUG
         fprintf(stderr, "Trapping result of %p\n",
                 next.input);
         print_result(result);
         #endif


         // TODO: add this to the hash table
         single_step_token(next.input, result.output_1, outgoing_token_packets, hash_table);
         
         if (result.marker == BOTH_OUTPUT_MARKER)
         {
            single_step_token(next.input, result.output_2, outgoing_token_packets, hash_table);
         }
      }
      else
      {
	     #ifdef DEBUG
         fprintf(stderr, "%d: Execution result of %d %s %lu %lu 0x%lx\n",
                 pid,
                 DESTINATION_TO_ADDRESS(next.input),
                 opcode_to_name[next.opcode],
                 next.data_1,
                 next.data_2,
                 next.tag);
         print_result(result);
         #endif

         send_result(result, outgoing_token_packets);
      }
   }
}

execution_result function_unit(execution_packet packet)
{
   data_type computation_result;
   execution_result output;
   output.marker = ONE_OUTPUT_MARKER;

   // Default is that the output tags are the same as the input tag
   output.output_1.tag = packet.tag;
   output.output_2.tag = packet.tag;
   
   switch(packet.opcode)
   {
	  // data_1 + data_2
	  case ADD:
		 computation_result = (data_type)((int64_t)packet.data_1 + (int64_t)packet.data_2);
		 break;
	  // data_1 - data_2
	  case SUB: 
		 computation_result = (data_type)((int64_t)packet.data_1 - (int64_t)packet.data_2);
		 break;
	  // data_1 < data_2
	  case LT:
		 if ((int64_t)packet.data_1 < (int64_t)packet.data_2)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;

	  // data_1 > data_2
	  case GT:
		 if ((int64_t)packet.data_1 > (int64_t)packet.data_2)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;

	  // data_1 >= data_2
	  case GTE:
		 if ((int64_t)packet.data_1 >= (int64_t)packet.data_2)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;

	  // data_1 <= data_2
	  case LTE:
		 if ((int64_t)packet.data_1 <= (int64_t)packet.data_2)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;
		 
	  // data_1 == data_2
	  case EQ:
		 if ((int64_t)packet.data_1 == (int64_t)packet.data_2)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;

	  // data_1 != data_2
	  case NEQ:
		 if ((int64_t)packet.data_1 == (int64_t)packet.data_2)
		 {
			computation_result = FALSE;
		 }
		 else
		 {
			computation_result = TRUE;
		 }
		 break;

	  // !data_1
	  case NEG:
		 if (packet.data_1 == FALSE)
		 {
			computation_result = TRUE;
		 }
		 else
		 {
			computation_result = FALSE;
		 }
		 break;
	  // duplication data_1 (data_2 ignored)
	  case DUP:
		 computation_result = packet.data_1;
		 break;

	  case BRR:
		 
		 /* BRR semantics:
			if (data_2)
			{
			  destination_1 = data_1;
			}
			else
			{
			  destination_2 = data_1;
			}
		 */
		 if (packet.data_2 != FALSE)
		 {
			output.output_1.destination = packet.destination_1;
			output.output_1.data = packet.data_1;
		 }
		 else
		 {
			output.output_1.destination = packet.destination_2;
			output.output_1.data = packet.data_1;
		 }
		 break;


	  // destination_1 = data_1
	  // The key idea here is that MERge takes two inputs and outputs the first that is ready.
	  // The matching_store will take care of making sure that whatever is ready is in the first packet.
	  case MER:
		 computation_result = packet.data_1;
		 break;

	  // data_1 = new_tag_area(), iteration_count = 0
	  // data_2 = data_1.tag
	  case NTG:
		 output.output_1.data = CREATE_TAG(new_tag_area(), 0);
		 output.output_1.destination = packet.destination_1;
		 output.output_1.tag = packet.tag;

		 output.output_2.destination = packet.destination_2;
		 output.output_2.data = packet.tag;
		 output.output_2.tag = packet.tag;

		 output.marker = BOTH_OUTPUT_MARKER;
		 break;

	  // output.tag = data_1.tag + 1
	  case ITG:
		 computation_result = packet.data_1;
		 output.output_1.tag = packet.tag + 1;
		 output.output_2.tag = output.output_1.tag;
		 break;

	  // output = data_1
	  // output.tag.iteration_count = data_2
	  case SIL:
		 computation_result = packet.data_1;
		 output.output_1.tag = CREATE_TAG(TAG_TO_TAG_AREA(packet.tag), (iteration_count_type)packet.data_2);
		 output.output_2.tag = output.output_1.tag;
		 break;

	  // output = data_2
	  // output.tag = data_1
	  case CTG:
		 computation_result = packet.data_2;
		 output.output_1.tag = packet.data_1;
		 output.output_2.tag = packet.data_1;
		 break;

	  // output = data_1
	  // destination_1 = data_2
	  case RTD:
		 output.output_1.data = packet.data_1;
		 output.output_1.destination = packet.data_2;
		 break;

	  // output = data_1.tag
	  case ETG:
		 computation_result = packet.tag;
		 break;

	  // output = data_1 * data_2
	  case MUL:
		 computation_result = packet.data_1 * packet.data_2;
		 break;

	  // output = data_1 ^ data_2
	  case XOR:
		 computation_result = packet.data_1 ^ packet.data_2;
		 break;

	  // output = data_1 & data_2
	  case AND:
		 computation_result = packet.data_1 & packet.data_2;
		 break;

	  // output = data_1 | data_2
	  case OR:
		 computation_result = packet.data_1 | packet.data_2;
		 break;

	  // output = data_1 << data_2
	  case SHL:
		 computation_result = packet.data_1 << packet.data_2;
		 break;

	  // output = data_2 >> data_1
	  case SHR:
		 computation_result = packet.data_1 >> packet.data_2;
		 break;

	  // Shutdown the system: exit(data_1);
	  case HLT:
		 exit(packet.data_1);
		 break;

      // return random()
      case RND:
         computation_result = (data_type) (random() ^ packet.tag ^ time(NULL));
         break;

	  // we should never get these instructions, they should be handled by other modules
	  case OPN:
	  case RED:
	  case WRT:
	  case CLS:
	  case LOD:
	  case LS:
      case SDF:
      case ULK:
      case LSK:
		 #ifdef DEBUG
		 assert(false);
		 #endif
		 break;
   }

   if (packet.opcode != BRR &&
	   packet.opcode != NTG &&
	   packet.opcode != RTD)
   {
	  /* pure computation, put the output correctly */
	  output.output_1.destination = packet.destination_1;
	  output.output_1.data =  computation_result;

	  if (packet.marker == BOTH_OUTPUT_MARKER)
	  {
		 output.output_2.destination = packet.destination_2;
		 output.output_2.data = computation_result;
		 output.marker = BOTH_OUTPUT_MARKER;
	  }
   }
   return output;
}
