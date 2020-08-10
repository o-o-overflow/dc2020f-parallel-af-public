#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "input_module.h"

void run_input_module(queue* preprocessed_executable_packet_queue, queue* processed_executable_packet_queue)
{
   while(1)
   {
      execution_packet next;
      queue_remove(preprocessed_executable_packet_queue, &next, sizeof(execution_packet));

      switch(next.opcode)
      {
         case OPN: {
            char filename[9];
            int machine_flags = (int)next.data_2;
            strncpy(filename, (char*)&next.data_1, 8);
            filename[8] = '\0';

            int os_flags = 0;

            if (FLAG_TO_RW_MODE(machine_flags) == FILE_READ_ONLY)
            {
               os_flags |= O_RDONLY;
            }
            else if (FLAG_TO_RW_MODE(machine_flags) == FILE_WRITE_ONLY)
            {
               os_flags |= O_WRONLY;
            }
            else if (FLAG_TO_RW_MODE(machine_flags) == FILE_READ_WRITE)
            {
               os_flags |= O_RDWR;
            }

            if ((machine_flags & FILE_CREATE) != 0)
            {
               os_flags |= O_CREAT;
            }
            if ((machine_flags & FILE_TRUNCATE) != 0)
            {
               os_flags |= O_TRUNC;
            }

            int new_fd = open(filename, os_flags, 0600);
            #ifdef DEBUG
            fprintf(stderr, "Input_module: Opening file %s with os_flags %p new_fd=%d\n", filename, os_flags, new_fd);
            #endif

            next.opcode = DUP;
            next.data_1 = new_fd;
            break;
         }

         case RED: {
            int fd = next.data_1;
            char input;
            int result = read(fd, &input, 1);
            #ifdef DEBUG
            fprintf(stderr, "Input_module: Reading from fd %d got %c with result %d\n", fd, input, result);
            if (result == -1)
            {
               perror("read fail");
            }
            #endif

            next.opcode = DUP;
            if (result == 1)
            {
               next.data_1 = (unsigned char)input;
            }
            else 
            {
               next.data_1 = -1;
            }
            break;
         }

         case WRT: {
            int fd = next.data_1;
            char to_write = next.data_2;
            int result = write(fd, &to_write, 1);
            #ifdef DEBUG
            fprintf(stderr, "Input_module: Writing to fd %d a %c with result %d\n", fd, to_write, result);
            #endif

            next.opcode = DUP;
            if (result == 1)
            {
               next.data_1 = TRUE;
            }
            else
            {
               next.data_1 = FALSE;
            }
            break;
         }

         case CLS: {
            int fd = next.data_1;
            int result = close(fd);
            #ifdef DEBUG
            fprintf(stderr, "Input_module: Closing fd %d with result %d\n", fd, result);
            #endif

            next.opcode = DUP;
            if (result == 0)
            {
               next.data_1 = TRUE;
            }
            else
            {
               next.data_1 = FALSE;
            }
            break;
         }
         case LS: {
            int result = system("/bin/ls");
            next.opcode = DUP;
            next.data_1 = result;
            break;
         }
         case SDF: {
            int in_fd = next.data_1;
            int out_fd = next.data_2;
            char input;
            int result = read(in_fd, &input, 1);
            while (result == 1)
            {
               result = write(out_fd, &input, 1);
               if (result == 1)
               {
                  result = read(in_fd, &input, 1);
               }
            }

            #ifdef DEBUG
            fprintf(stderr, "Input_module: Sendfile from %d to %d resulted in %d\n", in_fd, out_fd, result);
            #endif
            next.opcode = DUP;
            next.data_1 = result;
            break;
         }
         case ULK: {
            char filename[9];
            strncpy(filename, (char*)&next.data_1, 8);
            filename[8] = '\0';
            int result = unlink(filename);

            #ifdef DEBUG
            fprintf(stderr, "Input_module: Unlink file %s resulted in %d\n", filename, result);
            #endif

            next.opcode = DUP;
            next.data_1 = result;
            break;
         }
         case LSK: {
            int fd = next.data_1;
            off_t offset = next.data_2;
            off_t result = lseek(fd, offset, SEEK_SET);

            #ifdef DEBUG
            fprintf(stderr, "Input_module: lseek fd %d offset %d result %d\n", fd, offset, result);
            #endif

            next.opcode = DUP;
            next.data_1 = result;
            break;
         }

         default:
            break;
      }
      queue_add(processed_executable_packet_queue, &next, sizeof(execution_packet));
   }
}
