#ifndef IO_SWITCH_H
#define IO_SWITCH_H

#include "types.h"
#include "queue.h"

void run_io_switch(queue* execution_token_output_queue, queue* matching_unit_input_queue);

#endif /* IO_SWITCH_H */
