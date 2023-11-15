#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>

struct Node* createNode(pid_t pid, int* page_table) {
	struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
	if (newNode == NULL) {
		printf("Memory allocation failed\n");
		exit(EXIT_FAILURE);
	}

	newNode->data.pid = pid;
	newNode->data.page_table = page_table;
	newNode->next = NULL;

	return newNode;
}

void insert(struct Node* head, pid_t pid, int* page_table) {
	struct Node* newNode = createNode(pid,page_table);

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

struct Node* remove(struct Node* head, pid_t pid) {
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