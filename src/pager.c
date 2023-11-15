#include "pager.h"
#include "uvm.h"

struct process_data {
    pid_t pid;
    int* page_table;
};

struct pager_data {
    int nframes;
    int nblocks;
    int numProcesses;
    struct process_data* proccesses_datas;
};

struct pager_data pager_data;

void pager_init(int nframes, int nblocks) {
    uvm_create();
    pager_data.nframes = nframes;
    pager_data.nblocks = nblocks;
    pager_data.numProcesses = 0;

    struct process_data p;
    p.pid = -1;
    p.page_table = NULL;
    pager_data.proccesses_datas[0] = p;
}

void pager_create(pid_t pid) {
    struct process_data p;
    p.pid = pid;
    p.page_table = NULL;
    pager_data.proccesses_datas[pager_data.numProcesses];
    pager_data.numProcesses++;
}

void *pager_extend(pid_t pid) {

}

void pager_destroy(pid_t pid) {

}