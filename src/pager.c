/****************************************************************************
 * Imports
 ***************************************************************************/

#include "pager.h"
#include "uvm.h"
#include "mmu.h"

#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/****************************************************************************
 * Chained List Data Structure
 ***************************************************************************/
struct process_data {
	pid_t pid;
	int valid;
	int present_on_memory;
} process_data_t;

struct Node {
	struct process_data data;
	struct Node *next;
};

struct Node* createNode(pid_t pid) {
	struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
	if (newNode == NULL) {
		printf("Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	newNode->data.valid = 0;
	newNode->data.present_on_memory = 0;
	newNode->data.pid = pid;
	newNode->next = NULL;

	return newNode;
}

void insert(struct Node* head, pid_t pid) {
	struct Node* newNode = createNode(pid);

	if(head == NULL) {
		head = newNode;
	} else {
		struct Node* current = head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = newNode;
	}
}

struct Node* removeProcess(struct Node* head, pid_t pid) {
	struct Node* current = head;
	struct Node* previous = NULL;

	while (current != NULL) {
		if (current->data.pid == pid) {
			if (previous == NULL) {
				head = current->next;
			} else {
				previous->next = current->next;
			}
			free(current);
			return current;
		}
		previous = current;
		current = current->next;
	}

	return NULL;
}

struct Node* searchByPid(struct Node* head, pid_t pid) {
	struct Node* current = head;

	while (current != NULL) {
		if (current->data.pid == pid) {
			return current;
		}
		current = current->next;
	}

	return NULL;
}

void printList(struct Node* head) {
	struct Node* current = head;

	while (current != NULL) {
		printf("PID: %d\n", current->data.pid);
		current = current->next;
	}
}


/****************************************************************************
 * Pager Implementation
 ***************************************************************************/
typedef struct frame {
	pid_t pid;
	int occupied_frame;
} frame_t;


frame_t *frames_vector;
int frames_vector_size;
int free_frames;
frame_t *blocks_vector;
int blocks_vector_size;
int free_blocks;
struct Node* head_process;


void pager_init(int nframes, int nblocks) {
  if (nframes <= 0 || nblocks <= 0) {
    printf("Pager initialization failed\n");
		exit(EXIT_FAILURE);
  }

  frames_vector = malloc(nframes * sizeof(frame_t));
  blocks_vector = malloc(nblocks * sizeof(frame_t));
  free_frames = nframes;
  free_blocks = nblocks;
  frames_vector_size = nframes;
  blocks_vector_size = nblocks;
  for (int i = 0; i < nframes; i++) {
    frames_vector[i].pid = -1;
    frames_vector[i].occupied_frame = 0;
  }
  for (int i = 0; i < nblocks; i++) {
    blocks_vector[i].pid = -1;
    blocks_vector[i].occupied_frame = 0;
  }

  head_process = createNode(-1);
}

void pager_create(pid_t pid) {
  int num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

  int *page_table = malloc(num_pages * sizeof(int));
  for (int i = 0; i < num_pages; i++) {
    page_table[i] = -1;
  }

  if (page_table == NULL) {
    printf("Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  insert(head_process, pid);
}

void *pager_extend(pid_t pid) {
  if (free_blocks == 0 )
    return NULL;
  
  int frame = -1;

  for (int i=0; i < frames_vector_size; i++) {
    if (frames_vector[i].occupied_frame == 0) {
      frames_vector[i].occupied_frame = 1;
      free_frames--;
      frame = i;
      break;
    }
  }

  return (void*) (UVM_BASEADDR + (intptr_t) (frame * sysconf(_SC_PAGESIZE)));
}

void pager_destroy(pid_t pid) {
	for (int i = 0; i < frames_vector_size; i++) {
		if(frames_vector->pid == pid) {
			frames_vector->pid = -1;
			frames_vector->occupied_frame = 0;
			free_frames++;
		}
	} 

	for (int i = 0; i < blocks_vector_size; i++) {
		if(blocks_vector->pid == pid) {
			blocks_vector->pid = -1;
			blocks_vector->occupied_frame = 0;
			free_frames++;
		}
	} 

	removeProcess(head_process, pid);
}

void pager_fault(pid_t pid, void *addr) {

}

int pager_syslog(pid_t pid, void *addr, size_t len) {
  return 0;
}