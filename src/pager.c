#include "pager.h"
#include "uvm.h"
#include "linked_list.h"
#include "mmu.h"

#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct frame {
	pid_t pid;
	int page_number;
	int occupied_frame;
} frame_t;


frame_t *frames_vector;
int free_frames;
int *blocks_vector;
int free_blocks;
struct Node* head_process;


void pager_init(int nframes, int nblocks) {
  if (nframes <= 0 || nblocks <= 0) {
    printf("Pager initialization failed\n");
		exit(EXIT_FAILURE);
  }

  uvm_create();

  frames_vector = malloc(nframes * sizeof(frame_t));
  blocks_vector = malloc(nblocks * sizeof(int));
  free_frames = nframes;
  free_blocks = nblocks;
  for (int i = 0; i < nframes; i++) {
    frames_vector[i].pid = -1;
    frames_vector[i].page_number = -1;
    frames_vector[i].occupied_frame = 0;
  }
  for (int i = 0; i < nblocks; i++) {
    blocks_vector[i] = 0;
  }

  head_process = createNode(-1, NULL);
}

void pager_create(pid_t pid) {
  int num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

  int *page_table = malloc(num_pages * sizeof(int));

  if (page_table == NULL) {
    printf("Memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  insert(head_process, pid, page_table);
}

void *pager_extend(pid_t pid) {
  int *address = uvm_extend();
}

void pager_destroy(pid_t pid) {

}