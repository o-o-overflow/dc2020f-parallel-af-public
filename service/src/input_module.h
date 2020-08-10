#ifndef INPUT_MODULE_H
#define INPUT_MODULE_H

#include "types.h"
#include "queue.h"

#define FILE_READ_ONLY 0x0
#define FILE_WRITE_ONLY 0x1
#define FILE_READ_WRITE 0x2
#define FLAG_TO_RW_MODE(f) ((f & 0x3))

#define FILE_CREATE 0x10
#define FILE_TRUNCATE 0x20

void run_input_module(queue* preprocessed_executable_packet_queue, queue* processed_executable_packet_queue);

#endif /* INPUT_MODULE_H */
