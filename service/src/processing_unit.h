#ifndef PROCESSING_UNIT_H
#define PROCESSING_UNIT_H

#include "types.h"
#include "queue.h"

void run_processing_unit(queue* incoming_execution_packets, queue* outgoing_token_packets);
execution_result function_unit(execution_packet packet);

#endif /* PROCESSING_UNIT_H */
