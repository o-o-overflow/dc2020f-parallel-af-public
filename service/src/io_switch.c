#include <stdio.h>

#include "io_switch.h"

void run_io_switch(queue* execution_token_output_queue, queue* matching_unit_input_queue)
{
   token_type next_token;

   while (1)
   {
	  queue_remove(execution_token_output_queue, &next_token, sizeof(token_type));

	  switch(next_token.destination)
	  {
		 case OUTPUTD_DESTINATION:
			printf("%ld\n", next_token.data);
			fflush(stdout);
			break;

		 case OUTPUTS_DESTINATION:
			// vuln: could use this to leak out the next part of the token,
			// might be useful for exploitation.
			printf("%s", (char*)&next_token.data);
			fflush(stdout);
			break;

		 case REGISTER_INPUT_HANDLER_DESTINATION:
			#ifdef DEBUG
			fprintf(stderr, "TODO: unimplemented register input handler");
			print_token(next_token);
			#endif
			break;

		 case DEREGISTER_INPUT_HANDLER_DESTINATION:
			#ifdef DEBUG
			fprintf(stderr, "TODO: unimplemented deregister input handler");
			print_token(next_token);
			#endif
			break;

		 case DEV_NULL_DESTINATION:
			// Just consume the token, it's not meant for anywhere (/dev/null)
			#ifdef DEBUG
			fprintf(stderr, "Ignoring this token:\n");
			print_token(next_token);
			#endif
			break;

		 default:
			queue_add(matching_unit_input_queue, &next_token, sizeof(token_type));
	  }
   }
}
