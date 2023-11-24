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

/**
 * @struct least_frequently_pointer
 * @brief Represents a pointer to the least frequently used page in a process.
 * 
 * This struct holds a reference to the initial process and the initial page number
 * of the least frequently used page in that process.
 */
struct least_frequently_pointer {
  struct Node* initial_process; /**< Pointer to the initial process */
  int initial_page; /**< Initial page number */
};

/**
 * @struct page_table_cell
 * @brief Represents a cell in the page table.
 * 
 * This struct contains information about a page in the page table, including its validity,
 * presence in memory, protection level, recent access status, data availability, page number,
 * and corresponding frame number.
 */
struct page_table_cell {
  short valid;                /**< Flag indicating if the page is valid */
  short present;              /**< Flag indicating if the page is present in memory */
  short prot;                 /**< Protection level of the page */
  short recently_accessed;    /**< Flag indicating if the page was recently accessed */
  short has_data;             /**< Flag indicating if the page has data */
  __intptr_t page;            /**< Page number */
  int frame;                  /**< Frame number */
};

/**
 * @struct process_data
 * @brief Represents the data associated with a process.
 * 
 * This struct contains information such as the process ID, the number of frames allocated,
 * the queue the process belongs to, and a pointer to the page table.
 */
struct process_data {
  pid_t pid; /**< The process ID */
  size_t frames_allocated; /**< The number of frames allocated */
  short queue; /**< The queue the process belongs to */
  struct page_table_cell *page_table; /**< Pointer to the page table */
};

/**
 * @struct Node
 * Represents a node in a linked list.
 * Contains a process_data struct and a pointer to the next node.
 */
struct Node {
  struct process_data data;
  struct Node *next;
};

/**
 * Creates a new node for a linked list with the given process ID.
 * The node contains a page table and other data related to the process.
 *
 * @param pid The process ID of the new node.
 * @return A pointer to the newly created node.
 */
struct Node* createNode(pid_t pid) {
  // Allocate memory for the new node
  struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
  if (newNode == NULL) {
    printf("Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }

  // Initialize the page table for the process
  struct page_table_cell *page_table = malloc(NUM_PAGES * sizeof(struct page_table_cell));
  for (int i = 0; i < NUM_PAGES; i++) {
    page_table[i].valid = 0;
    page_table[i].present = 0;
    page_table[i].recently_accessed = 0;
    page_table[i].has_data = 0;
    page_table[i].page = -1;
    page_table[i].frame = -1;
  }

  // Set the data of the new node
  newNode->data.page_table = page_table;
  newNode->data.pid = pid;
  newNode->data.frames_allocated = 0;
  newNode->data.queue = 0;
  newNode->next = NULL;

  return newNode;
}

/**
 * Inserts a new node with the given process ID at the end of the linked list.
 *
 * @param head Pointer to the head of the linked list.
 * @param pid Process ID to be inserted.
 */
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

/**
 * Removes a process node from a linked list based on the given process ID.
 *
 * @param head Pointer to the head of the linked list.
 * @param pid The process ID to be removed.
 * @return Pointer to the removed node, or NULL if the node was not found.
 */
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

/**
 * Searches for a node with a specific process ID (pid) in a linked list.
 * 
 * @param head The head of the linked list.
 * @param pid The process ID to search for.
 * @return A pointer to the node with the specified pid, or NULL if not found.
 */
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

/**
 * Retrieves the next node in a linked list.
 * If the current node has a next node, it returns the next node.
 * Otherwise, it returns the head of the linked list.
 *
 * @param process The current node in the linked list.
 * @param head The head of the linked list.
 * @return The next node in the linked list.
 */
struct Node* getNextNode(struct Node* process, struct Node* head) {
  if (process->next != NULL) return process->next;
  else return head;
}

/**
 * Searches for a page in the page table of a process with the given PID,
 * based on the virtual address.
 *
 * @param head The head of the linked list containing the process nodes.
 * @param pid The process ID of the target process.
 * @param virtual_addr The virtual address to search for.
 * @return A pointer to the page table cell if found, NULL otherwise.
 */
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

/**
 * Searches for the least frequently used frame index in a linked list of processes.
 * Uses the known second chance algorithm to find the least frequently used frame.
 * 
 * @param head The head of the linked list of processes.
 * @param pointer A pointer to a struct that keeps track of the initial process and page index.
 * @return A pointer to the struct containing the updated initial process and page index.
 */
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

/**
 * Prints the data of each node in a linked list.
 *
 * @param head The head of the linked list.
 */
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

/**
 * @struct frame
 * Represents a frame in the memory pager.
 * Each frame contains the process ID (pid) of the process currently occupying the frame.
 */

/**
 * @file pager.c
 * @brief Implementation of a memory pager.
 *
 * This file contains the implementation of a memory pager, which manages
 * the allocation and deallocation of memory frames and blocks. It also
 * includes data structures and variables used for tracking the state of
 * the memory pager.
 */

typedef struct frame {
  pid_t pid;  /**< The process ID of the process occupying the frame. */
} frame_t;

int *frames_vector;                 /**< Array of memory frames */
int frames_vector_size;             /**< Size of the frames_vector array */
int free_frames;                    /**< Number of free memory frames */
int *blocks_vector;                 /**< Array of memory blocks */
int blocks_vector_size;             /**< Size of the blocks_vector array */
int free_blocks;                    /**< Number of free memory blocks */
struct least_frequently_pointer default_least_frequently_pointer = {NULL, -1};   /**< Default least frequently used frame pointer */
struct least_frequently_pointer* last_freed_frame_addr = &default_least_frequently_pointer;   /**< Address of the last freed frame */
pid_t mutex_turn = -1;              /**< Mutex turn identifier */
static pthread_mutex_t locker;      /**< Mutex locker */
struct Node* head_process = NULL;   /**< Head of the process linked list */

/**
 * Initializes the pager with the specified number of frames and blocks.
 *
 * @param nframes The number of frames.
 * @param nblocks The number of blocks.
 */
void pager_init(int nframes, int nblocks) {
  pthread_mutex_init(&locker, NULL);
  pthread_mutex_trylock(&locker);
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
  pthread_mutex_unlock(&locker);
}

/**
 * Creates a pager for the specified process.
 *
 * @param pid The process ID.
 */
void pager_create(pid_t pid) {
  pthread_mutex_trylock(&locker);
  insert(&head_process, pid);
  pthread_mutex_unlock(&locker);
}

/**
 * Extends the memory for a given process.
 *
 * @param pid The process ID.
 * @return A pointer to the allocated memory, or NULL if no free blocks are available.
 */
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

/**
 * @brief Destroys a pager for a given process ID.
 * 
 * This function releases the resources associated with the pager for the specified process ID.
 * It frees the frames and blocks occupied by the process, removes the process from the linked list,
 * and destroys the mutex lock if there are no more processes in the system.
 * 
 * @param pid The process ID of the pager to be destroyed.
 */
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

/**
 * Handles swapping of pages between memory and disk.
 * 
 * @param process_node Pointer to the process node in the page table.
 * @param cell_idx Index of the page table cell to be swapped.
 */
void _handleSwap(struct Node *process_node, int cell_idx, int has_empty_frame) {
  struct page_table_cell *page_cell = &process_node->data.page_table[cell_idx];
  
  int new_frame = -1;
  if(!has_empty_frame) {
    last_freed_frame_addr = searchLeastFrequentlyUsedFrameIdx(head_process, last_freed_frame_addr);
    struct page_table_cell *last_freed_cell = &last_freed_frame_addr->initial_process->data.page_table[last_freed_frame_addr->initial_page];
    mmu_nonresident(last_freed_frame_addr->initial_process->data.pid, (void *) last_freed_cell->page);
    if(last_freed_cell->has_data) {
      mmu_disk_write(last_freed_cell->frame, last_freed_cell->frame);
    }

    last_freed_cell->present = 0;
    new_frame = last_freed_cell->frame;
  } else {
    for (int i=0; i < frames_vector_size; i++) {
      if (frames_vector[i] == -1) {
        frames_vector[i] = process_node->data.pid;
        free_frames--;
        new_frame = i;
        break;
      }
    }
  }

  
  if(page_cell->has_data) {
    mmu_disk_read(page_cell->frame, new_frame);
  } else {
    mmu_zero_fill(new_frame);
  }

  page_cell->frame = new_frame;
  mmu_resident(process_node->data.pid, (void *) page_cell->page, page_cell->frame, PROT_READ);
  page_cell->prot = PROT_READ;
  page_cell->recently_accessed = 1;
}

/**
 * Handles a page fault for a specific process.
 *
 * @param pid The process ID.
 * @param addr The virtual address that caused the page fault.
 */
void pager_fault(pid_t pid, void *addr) {
  // Attempt to acquire the locker mutex
  pthread_mutex_trylock(&locker);

  // Search for the process node in the linked list
  struct Node *process_node = searchByPid(head_process, pid);

  // Iterate through the page table of the process
  for (int i = 0; i < NUM_PAGES; i++) {
    struct page_table_cell *page_cell = &process_node->data.page_table[i];
    __intptr_t virtual_addr = (intptr_t) addr;
   
    // Check if the virtual address falls within the page cell range
    short address_is_at_page_cell = page_cell->page <= virtual_addr && virtual_addr < page_cell->page + sysconf(_SC_PAGESIZE);
    
    if(address_is_at_page_cell) {
      // Handle the case when the page is not valid
      if(page_cell->valid == 0) {
        if(free_frames > 0) {
          // Find a free frame in the frames vector
          for (int i=0; i < frames_vector_size; i++) {
            if (frames_vector[i] == -1) {
              frames_vector[i] = pid;
              free_frames--;
              page_cell->frame = i;
              break;
            }
          }

          // Zero-fill the frame and make it resident
          mmu_zero_fill(page_cell->frame);
          mmu_resident(pid, (void *) page_cell->page, page_cell->frame, PROT_READ);
          page_cell->prot = PROT_READ;
          page_cell->recently_accessed = 1;
        } else {
          // Handle the case when there are no free frames available
          _handleSwap(process_node, i, 0);
        }
        
        page_cell->valid = 1;
        page_cell->present = 1;
        process_node->data.frames_allocated++;
        break;
      } 
      // Handle the case when the page is already present
      else if(page_cell->present == 1) {
        if(page_cell->prot == PROT_NONE) {
          // Change the protection of the page to read-only
          mmu_chprot(pid, (void *) page_cell->page, PROT_READ);
          page_cell->prot = PROT_READ;
        } else if(page_cell->prot == PROT_READ) {
          // Change the protection of the page to read-write
          mmu_chprot(pid, (void *) page_cell->page, PROT_READ | PROT_WRITE);
          page_cell->prot = PROT_READ | PROT_WRITE;
          page_cell->has_data = 1;
        }
        page_cell->recently_accessed = 1;
      } 
      // Handle the case when the page is not present
      else if (page_cell->present == 0) {
        _handleSwap(process_node, i, (free_frames > 0));
        page_cell->present = 1;
        break;
      } 
    }

    // Break the loop if the page cell is empty
    if (page_cell->page == -1) break;
  }

  // Release the locker mutex
  pthread_mutex_unlock(&locker);
}

/**
 * @brief Retrieves the contents of a memory region specified by the given process ID and address.
 * 
 * This function retrieves the contents of a memory region specified by the process ID and address.
 * It locks a mutex to ensure thread safety and returns the status of the syslog operation.
 * If the address is NULL, the function returns 0 without performing any operation.
 * If the memory region is present in the page table, it calculates the physical address and prints the contents.
 * The function returns 0 if the syslog operation is successful, otherwise -1.
 * 
 * @param pid The process ID of the target process.
 * @param addr The starting address of the memory region.
 * @param len The length of the memory region.
 * @return int The status of the syslog operation. 0 if successful, -1 otherwise.
 */
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