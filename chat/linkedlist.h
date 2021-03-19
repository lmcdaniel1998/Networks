/*********************************************************/
/* Header for Linked List in Program 2 Chat
 * Author: Luke McDaniel
 * Institution: California Polytechnic State University
 * Course: CPE 464-01 (Networks)
*/
/*********************************************************/
#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__

#include <sys/socket.h>

#define MAXLOAD 0.20

typedef struct socketHandle {
	char handle[101];
	unsigned long key;
	int socketNum;
	struct socketHandle* prev;
	struct socketHandle* next;
}socketHandle;

struct socketHandle* getFirst(void);
int getNumItems(void);
struct socketHandle* getLL(char* handle, int socketNum);
void addLL(struct socketHandle* client);
void removeLL(struct socketHandle* client);
void printLL(void);
#endif
