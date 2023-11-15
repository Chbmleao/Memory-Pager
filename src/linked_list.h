#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <sys/types.h>

struct process_data {
    pid_t pid;
    int* page_table;
};

struct Node {
    struct process_data data;
    struct Node* next;
}

struct Node* createNode(pid_t pid, int* page_table);
void insert(struct Node* head, pid_t pid, int* page_table);
struct Node* searchByPid(struct Node* head, pid_t);
void printList(struct Node* head);
void freeList(struct Node* head);

#endif