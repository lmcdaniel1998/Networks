/*********************************************************/
/* Linked List for Program 2 Chat
 * Author: Luke McDaniel
 * Institution: California Polytechnic State University
 * Course: CPE 464-01 (Networks)
*/
/*********************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "linkedlist.h"

int NumItems = 0;
struct socketHandle* first = NULL;
struct socketHandle* last = NULL;

struct socketHandle* getFirst(void) {
	return first;
}

int getNumItems(void) {
	return NumItems;
}
/* pass handle = NULL if using socketNum to find
 * pass socketNum = -1 if using handle to find */
struct socketHandle* getLL(char* handle, int socketNum) {
	struct socketHandle* temp = first;
	while (temp != NULL) {
		/* match found */
		if (socketNum == -1) {
			if (strcmp(handle, &(temp->handle[0])) == 0) {
				return temp;
			}
		}
		if (handle == NULL) {
			if (socketNum == temp->socketNum) {
				return temp;
			}
		}
		temp = temp->next;
	}
	/* returns NULL if not found */
	return temp;
}

/* adds socketHandle to linked list */
void addLL(struct socketHandle* client) {
	/* first entry in list */
	if (first == NULL && last == NULL) {
		first = client;
		last = client;
		first->next = NULL;
		first->prev = NULL;
		last->next = NULL;
		last->prev = NULL;
	}
	else {
		last->next = client;
		client->prev = last;
		client->next = NULL;
		last = client;
	}
	NumItems++;
}

/* removes socketHandle from linked list */
void removeLL(struct socketHandle* client) {
	struct socketHandle* temp;
	/* client is only one it table */
	if (client == first && client == last) {
		first = NULL;
		last = NULL;
	}
	/* client is first but not last */
	else if (client == first && client != last) {
		client->next->prev = NULL;
		first = client->next;
		client->next = NULL;
	}
	/* client is last but not first */
	else if (client != first && client == last) {
		client->prev->next = NULL;
		last = client->prev;
		client->prev = NULL;
	}
	/* client is not fisrt or last */
	else {
		temp = client->prev;
		client->prev->next = client->next;
		client->next->prev = temp;
		client->next = NULL;
		client->prev = NULL;
	}
	NumItems--;
	client = NULL;
}

void printLL(void) {
	struct socketHandle* temp = first;
	if (first == NULL && last == NULL) {
		printf("no entries LL.\n");
		return;
	}
	printf("entries LL:\n");
	while (temp != NULL) {
		printf("%s\n", &(temp->handle[0]));
		temp = temp->next;
	}
}
