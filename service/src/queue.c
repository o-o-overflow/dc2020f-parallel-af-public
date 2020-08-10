#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>

#include <pthread.h>

#include "queue.h"

// Basing this on https://github.com/goldshtn/shmemq-blog/blob/master/shmemq.c

typedef struct {
   pthread_mutex_t lock;
   char* head;
   char* tail;
   char data[1];
} shared_queue;

struct _queue {
   unsigned long max_count;
   unsigned int element_size;
   unsigned long max_size;
   char* name;
   int shmem_fd;
   unsigned long mmap_size;
   sem_t* can_write_lock;
   sem_t* can_read_lock;   
   shared_queue* mem;
};

queue* queue_new(char* name, unsigned long max_count, unsigned int element_size)
{
   queue* to_return;

   to_return = (queue*)malloc(sizeof(queue));
   to_return->max_count = max_count;
   to_return->element_size = element_size;
   to_return->max_size = max_count * element_size;
   to_return->name = strdup(name);
   to_return->mmap_size = to_return->max_size + sizeof(shared_queue) - 1;

   // Create the shared memory region
   if ((to_return->shmem_fd = shm_open(to_return->name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR)) == -1)
   {
	  #ifdef DEBUG
	  perror("shm_open fail");
	  #endif
	  goto FAIL;
   }
   if (shm_unlink(to_return->name) == -1)
   {
	  #ifdef DEBUG
	  perror("shm_unlink fail");
	  #endif
	  goto FAIL;
   }

   if (ftruncate(to_return->shmem_fd, to_return->mmap_size) == -1)
   {
	  #ifdef DEBUG
	  perror("ftruncate fail");
	  #endif
	  goto FAIL;
   }

   // Create the necessary semaphores
   if ((to_return->can_write_lock = sem_open(name, O_CREAT, S_IRUSR | S_IWUSR, max_count)) == SEM_FAILED)
   {
	  #ifdef DEBUG
	  perror("sem_open");
	  #endif
	  goto FAIL;
   }
   if (sem_unlink(name) == -1)
   {
	  #ifdef DEBUG
	  perror("sem_unlink");
	  #endif
	  goto FAIL;
   }

   // Create the necessary semaphores
   if ((to_return->can_read_lock = sem_open(name, O_CREAT, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
   {
	  #ifdef DEBUG
	  perror("sem_open");
	  #endif
	  goto FAIL;
   }
   if (sem_unlink(name) == -1)
   {
	  #ifdef DEBUG
	  perror("sem_unlink");
	  #endif
	  goto FAIL;
   }
   

   if ((to_return->mem = (shared_queue*) mmap(NULL, to_return->mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, to_return->shmem_fd, 0)) == MAP_FAILED)
   {
	  #ifdef DEBUG
	  perror("mmap fail");
	  #endif
	  goto FAIL;
   }

   to_return->mem->head = to_return->mem->data;
   to_return->mem->tail = to_return->mem->data;

   {
	  pthread_mutexattr_t attr;
	  pthread_mutexattr_init(&attr);
	  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	  // pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);
	  pthread_mutex_init(&to_return->mem->lock, &attr);
	  pthread_mutexattr_destroy(&attr);
   }

   return to_return;

  FAIL:
   if (to_return->shmem_fd != -1)
   {
	  close(to_return->shmem_fd);
	  shm_unlink(to_return->name);
   }

   free(to_return->name);
   free(to_return);
   return NULL;
}

void queue_free(queue* ptr)
{
   munmap(ptr->mem, ptr->mmap_size);
   close(ptr->shmem_fd);
   free(ptr->name);
   free(ptr);
}

bool queue_add(queue* ptr, void* element, unsigned int len)
{
   #ifdef DEBUG
   assert(len == ptr->element_size);
   #endif

   // check if there's enough space
   // B/C can_write_lock is instantiated with the 
   sem_wait(ptr->can_write_lock);
   
   {
	  char* next;
	  pthread_mutex_lock(&ptr->mem->lock);
	  memcpy((void*)ptr->mem->tail, element, len);

	  // fix up the queue, wrap around as necessary
	  next = ptr->mem->tail + (ptr->element_size);

	  // Are we at the end of the queue?
	  if (next == (ptr->mem->data + ptr->max_size))
	  {
		 next = ptr->mem->data;
	  }
	  ptr->mem->tail = next;
	  pthread_mutex_unlock(&ptr->mem->lock);
   }

   // Let readers know that there's something to read
   sem_post(ptr->can_read_lock);
   return true;
}

bool queue_remove(queue* ptr, void* element, unsigned int len)
{
   #ifdef DEBUG
   assert(len == ptr->element_size);
   #endif

   // wait until there's actually something to read
   sem_wait(ptr->can_read_lock);

   {
	  char* next;
	  pthread_mutex_lock(&ptr->mem->lock);
	  
	  memcpy(element, (void*)ptr->mem->head, len);

	  next = ptr->mem->head + (ptr->element_size);

	  // are we at the end of the queue?
	  if (next == (ptr->mem->data + ptr->max_size))
	  {
		 next = ptr->mem->data;
	  }
	  ptr->mem->head = next;
	  pthread_mutex_unlock(&ptr->mem->lock);	  
   }

   // Let writers know that there's space to write
   sem_post(ptr->can_write_lock);
   return true;
}

/* int main(int argc, char** argv) */
/* { */
/*    int test = 100; */
/*    queue* queue = queue_new("/testing", 1000, sizeof(int)); */

/*    int pid = fork(); */

/*    if (pid == 0) */
/*    { */
/* 	  int i = 0; */
/* 	  while (true) */
/* 	  { */
/* 		 printf("About to add %d\n", i); */
/* 		 queue_add(queue, &i, sizeof(int)); */
/* 		 printf("Added %d\n", i); */
/* 		 i += 1; */
/* 	  } */
/*    } */
/*    else */
/*    { */
/* 	  fork(); */
/* 	  fork(); */
/* 	  while (true) */
/* 	  { */
/* 		 printf("Going to fetch from the queue\n"); */
/* 		 queue_remove(queue, &test, sizeof(int)); */
/* 		 printf("Got %d\n", test); */
/* 	  } */
/*    } */

/*    // queue_remove(queue, &test, sizeof(int)); */
/*    // queue_add(queue, &test, sizeof(int)); */
/*    //queue_add(queue, &test, sizeof(int)); */

   

/*    queue_free(queue); */
/* } */
