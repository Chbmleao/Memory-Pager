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

#define NUM_PAGES (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE)

/****************************************************************************
 * Chained List Data Structure
 ***************************************************************************/
struct page_table_cell {
  short valid;
  short present;
  __intptr_t page;
  int frame;
};

struct process_data {
  pid_t pid;
  struct page_table_cell *page_table;
};

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

  struct page_table_cell *page_table = malloc(NUM_PAGES * sizeof(struct page_table_cell));
  for (int i = 0; i< NUM_PAGES; i++) {
    page_table[i].valid = 0;
    page_table[i].present = 0;
    page_table[i].page = -1;
    page_table[i].frame = -1;
  }
  newNode->data.page_table = page_table;
  newNode->data.pid = pid;
	newNode->next = NULL;

	return newNode;
}

void insert(struct Node** head, pid_t pid) {
  struct Node* newNode = createNode(pid);

  if(*head == NULL) {
    *head = newNode;
  } else {
    struct Node* current = *head;
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

struct page_table_cell* searchByPage(struct Node* head, pid_t pid, __intptr_t page) {
  struct Node* pid_proccess = searchByPid(head, pid);
  
  if(pid_proccess == NULL) return NULL;
  
  for (int i = 0; i < NUM_PAGES; i++) {
    if (pid_proccess->data.page_table[i].page == page) {
      return &pid_proccess->data.page_table[i];
    }
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
} frame_t;


int *frames_vector;
int frames_vector_size;
int free_frames;
int *blocks_vector;
int blocks_vector_size;
int free_blocks;
struct Node* head_process = NULL;


void pager_init(int nframes, int nblocks) {
  if (nframes <= 0 || nblocks <= 0) {
    printf("Pager initialization failed\n");
		exit(EXIT_FAILURE);
  }

  frames_vector = malloc(nframes * sizeof(int));
  blocks_vector = malloc(nblocks * sizeof(int));
  free_frames = nframes;
  free_blocks = nblocks;
  frames_vector_size = nframes;
  blocks_vector_size = nblocks;
  for (int i = 0; i < nframes; i++) {
    frames_vector[i] = -1;
  }
  for (int i = 0; i < nblocks; i++) {
    blocks_vector[i] = -1;
  }
}

void pager_create(pid_t pid) {
  insert(&head_process, pid);
}

void *pager_extend(pid_t pid) {
  if (free_blocks == 0)
    return NULL;

  int free_idx = -1;
  if (free_frames == 0)  {
    // TODO: implement second-chance algorithm
  } else {
    for (int i=0; i < frames_vector_size; i++) {
      if (frames_vector[i] == -1) {
        frames_vector[i] = pid;
        free_idx = i;
        free_frames--;
        break;
      }
    }
  }

  struct Node *process_node = searchByPid(head_process, pid);
  if (process_node == NULL) {
    printf("Process not found\n");
    exit(EXIT_FAILURE);
  }

  for (int i=0; i < NUM_PAGES; i++) {
    if (process_node->data.page_table[i].page == -1) {
      __intptr_t virtual_address = UVM_BASEADDR + (intptr_t) (i * sysconf(_SC_PAGESIZE));
      process_node->data.page_table[i].page = virtual_address;
      process_node->data.page_table[i].frame = free_idx;
      return (void*) virtual_address;
    }
  }

  return NULL;
}

void pager_destroy(pid_t pid) {
  struct Node *process_node = searchByPid(head_process, pid);
  
  if(process_node == NULL) return;

  for (int i = 0; i < NUM_PAGES; i++) {
    int remove_idx = process_node->data.page_table[i].frame;
    if (remove_idx == -1) 
      break;
    
    frames_vector[remove_idx] = -1;
    free_frames++;
  }

	for (int i = 0; i < blocks_vector_size; i++) {
		if(blocks_vector[i] == pid) {
			blocks_vector[i] = -1;
			free_frames++;
		}
	}

	removeProcess(head_process, pid);
}

void pager_fault(pid_t pid, void *addr) {
  struct Node *process_node = searchByPid(head_process, pid);

  for (int i = 0; i < NUM_PAGES; i++) {
    struct page_table_cell *page_cell = &process_node->data.page_table[i];
    if (page_cell->page == (intptr_t) addr) {
      if(page_cell->valid == 0) {
        mmu_zero_fill(page_cell->frame);
        mmu_resident(pid, (void *) page_cell->page, page_cell->frame, PROT_READ);
        page_cell->valid = 1;
        page_cell->present = 1;
        return;
      } else if (page_cell->present == 1) {
        mmu_chprot(pid, (void *) page_cell->page, PROT_READ | PROT_WRITE);
      } else if (page_cell->present == 0) {
        return;
      } 
    }
  }
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
  if(addr == NULL) return 0;

  struct page_table_cell *page_table_cell = searchByPage(head_process, pid, (intptr_t) addr);
  int physical_address = page_table_cell->frame * sysconf(_SC_PAGESIZE);

  for(int i = physical_address; i < physical_address + len; i++) {
    printf("%02x", (unsigned)pmem[i]);
  }
  printf("\n");

  return 0;
}