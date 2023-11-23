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

struct least_frequently_pointer {
  struct Node* initial_process;
  int initial_page;
};

struct page_table_cell {
  short valid;
  short present;
  short prot;
  short recently_accessed;
  short has_data;
  __intptr_t page;
  int frame;
};

struct process_data {
  pid_t pid;
  size_t frames_allocated;
  short queue;
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
  for (int i = 0; i < NUM_PAGES; i++) {
    page_table[i].valid = 0;
    page_table[i].present = 0;
    page_table[i].recently_accessed = 0;
    page_table[i].has_data = 0;
    page_table[i].page = -1;
    page_table[i].frame = -1;
  }
  newNode->data.page_table = page_table;
  newNode->data.pid = pid;
  newNode->data.frames_allocated = 0;
  newNode->data.queue = 0;
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

struct Node* removeProcess(struct Node** head, pid_t pid) {
	struct Node* current = *head;
	struct Node* previous = NULL;

	while (current != NULL) {
		if (current->data.pid == pid) {
			if (previous == NULL) {
				*head = current->next;
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

struct Node* getNextNode(struct Node* process, struct Node* head) {
  if (process->next != NULL) return process->next;
  else return head;
}

struct page_table_cell* searchByPage(struct Node* head, pid_t pid, __intptr_t virtual_addr) {
  struct Node* pid_proccess = searchByPid(head, pid);
  
  if(pid_proccess == NULL) return NULL;
  
  for (int i = 0; i < NUM_PAGES; i++) {
    struct page_table_cell* page_cell = &pid_proccess->data.page_table[i];
    short address_is_at_page_cell = page_cell->page <= virtual_addr && virtual_addr < page_cell->page + sysconf(_SC_PAGESIZE);

    if (address_is_at_page_cell) {
      return page_cell;
    }

    if(page_cell->page == -1) break;
  } 

  return NULL;
}

struct least_frequently_pointer* searchLeastFrequentlyUsedFrameIdx(struct Node* head, struct least_frequently_pointer* pointer) {
  struct Node* process = head;
  if(pointer->initial_process != NULL) process = pointer->initial_process;

  size_t i = pointer->initial_page;
  while (1) {
    while(process->data.frames_allocated == 0) {
      process = getNextNode(process, head);
    }
    i = (i + 1) % process->data.frames_allocated;

    if(i == pointer->initial_page) {
      process = getNextNode(process, head);
      if(process == pointer->initial_process) {
        pointer->initial_page = (pointer->initial_page + 1) % process->data.frames_allocated;
        return pointer;
      };

      i = -1;
      pointer->initial_page = -1;
      continue;
    }

    if(process->data.page_table[i].present == 1) {
      if (process->data.page_table[i].recently_accessed == 0) {
        pointer->initial_process = process;
        pointer->initial_page = i;
        return pointer;
      }

      mmu_chprot(process->data.pid, (void *) process->data.page_table[i].page, PROT_NONE);
      process->data.page_table[i].prot = PROT_NONE;
      process->data.page_table[i].recently_accessed = 0;
    }

    if(pointer->initial_page == -1) pointer->initial_page = 0;
  }
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
struct least_frequently_pointer default_least_frequently_pointer = {NULL, -1};
struct least_frequently_pointer* last_freed_frame_addr = &default_least_frequently_pointer;
pid_t mutex_turn = -1;
static pthread_mutex_t locker;
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
  pthread_mutex_init(&locker, NULL);
}

void pager_create(pid_t pid) {
  insert(&head_process, pid);
}

void *pager_extend(pid_t pid) {
  pthread_mutex_trylock(&locker);
  
  if (free_blocks == 0) {
    pthread_mutex_unlock(&locker);
    return NULL;
  }

  for (int i=0; i < blocks_vector_size; i++) {
    if(blocks_vector[i] == -1) {
      blocks_vector[i] = pid;
      free_blocks--;
      break;
    }
  }

  struct Node *process_node = searchByPid(head_process, pid);
  if (process_node == NULL) {
    pthread_mutex_unlock(&locker);
    exit(EXIT_FAILURE);
  }

  for (int i=0; i < NUM_PAGES; i++) {
    if (process_node->data.page_table[i].page == -1) {
      __intptr_t virtual_address = UVM_BASEADDR + (intptr_t) (i * sysconf(_SC_PAGESIZE));
      process_node->data.page_table[i].page = virtual_address;
      pthread_mutex_unlock(&locker);
      return (void*) virtual_address;
    }
  }

  pthread_mutex_unlock(&locker);
  return NULL;
}

void pager_destroy(pid_t pid) {
  pthread_mutex_trylock(&locker);
  struct Node *process_node = searchByPid(head_process, pid);
  
  if(process_node == NULL) {
    pthread_mutex_unlock(&locker);
    return;
  };

  for (int i = 0; i < frames_vector_size; i++) {  
    if(frames_vector[i] == pid) {
      frames_vector[i] = -1;
      free_frames++;
    }
  }

	for (int i = 0; i < blocks_vector_size; i++) {
		if(blocks_vector[i] == pid) {
			blocks_vector[i] = -1;
			free_blocks++;
		}
	}

	removeProcess(&head_process, pid);
  pthread_mutex_unlock(&locker);
  if(head_process == NULL) {
    free(frames_vector);
    free(blocks_vector);
    pthread_mutex_destroy(&locker);
  }
}

void _handleSwap(struct Node *process_node, int cell_idx) {
  struct page_table_cell *page_cell = &process_node->data.page_table[cell_idx];
  
  last_freed_frame_addr = searchLeastFrequentlyUsedFrameIdx(head_process, last_freed_frame_addr);
  struct page_table_cell *last_freed_cell = &last_freed_frame_addr->initial_process->data.page_table[last_freed_frame_addr->initial_page];

  mmu_nonresident(last_freed_frame_addr->initial_process->data.pid, (void *) last_freed_cell->page);
  
  if(last_freed_cell->has_data) {
    mmu_disk_write(last_freed_cell->frame, last_freed_cell->frame);
  }
  if(page_cell->has_data) {
    mmu_disk_read(page_cell->frame, last_freed_cell->frame);
  } else {
    mmu_zero_fill(last_freed_cell->frame);
  }
  last_freed_cell->present = 0;

  page_cell->frame = last_freed_cell->frame;
  mmu_resident(process_node->data.pid, (void *) page_cell->page, page_cell->frame, PROT_READ);
  page_cell->prot = PROT_READ;
  page_cell->recently_accessed = 1;
}

void pager_fault(pid_t pid, void *addr) {
  pthread_mutex_trylock(&locker);
  struct Node *process_node = searchByPid(head_process, pid);

  for (int i = 0; i < NUM_PAGES; i++) {
    struct page_table_cell *page_cell = &process_node->data.page_table[i];
    __intptr_t virtual_addr = (intptr_t) addr;
   
    short address_is_at_page_cell = page_cell->page <= virtual_addr && virtual_addr < page_cell->page + sysconf(_SC_PAGESIZE);
    
    if (address_is_at_page_cell) {
      if(page_cell->valid == 0) {
        if(free_frames > 0) {
          for (int i=0; i < frames_vector_size; i++) {
            if (frames_vector[i] == -1) {
              frames_vector[i] = pid;
              free_frames--;
              page_cell->frame = i;
              break;
            }
          }

          mmu_zero_fill(page_cell->frame);
          mmu_resident(pid, (void *) page_cell->page, page_cell->frame, PROT_READ);
          page_cell->prot = PROT_READ;
          page_cell->recently_accessed = 1;
        } else {
          _handleSwap(process_node, i);
        }
        
        page_cell->valid = 1;
        page_cell->present = 1;
        process_node->data.frames_allocated++;
        break;
      } else if (page_cell->present == 1) {
        if(page_cell->prot == PROT_NONE) {
          mmu_chprot(pid, (void *) page_cell->page, PROT_READ);
          page_cell->prot = PROT_READ;
        } else if(page_cell->prot == PROT_READ) {
          mmu_chprot(pid, (void *) page_cell->page, PROT_READ | PROT_WRITE);
          page_cell->prot = PROT_READ | PROT_WRITE;
        }
        page_cell->has_data = 1;
        page_cell->recently_accessed = 1;
      } else if (page_cell->present == 0) {
        _handleSwap(process_node, i);
        page_cell->present = 1;
        break;
      } 
    }

    if(page_cell->page == -1) break;
  }

  pthread_mutex_unlock(&locker);
}

int pager_syslog(pid_t pid, void *addr, size_t len) {
  pthread_mutex_trylock(&locker);
  int syslog_status = 0;

  if(addr == NULL) return syslog_status;
 

  struct page_table_cell *page_table_cell = searchByPage(head_process, pid, (intptr_t) addr);
  if(page_table_cell != NULL && page_table_cell->present) {
    __intptr_t shift = (intptr_t) addr - page_table_cell->page;
    long physical_address = (page_table_cell->frame * sysconf(_SC_PAGESIZE)) + shift;

    for(long i = physical_address; i < physical_address + len; i++) {
      printf("%02x", (unsigned)pmem[i]);
    }
    printf("\n");

    syslog_status = 0;
  } else syslog_status = -1;
  
  pthread_mutex_unlock(&locker);
  return syslog_status;
}