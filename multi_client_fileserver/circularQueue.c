/*****************************************************************************/
 /* 
 * Author: Luke McDaniel
 * Institution: California Polytechnic State University
 * Course: CPE464 (Networks)
 * Project: File Server (Program 3)
 * Purpose: Circular Queue for both rcopy and server window buffers
 */
/*****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "circularQueue.h"

/* global window pointer */
struct Frame *Queue;
/* global meta data structure */
struct MetaData *md;
/* global counter for items in struct */
uint32_t numItems = 0;

uint32_t getCurrent(void) {
	return md->current;
}

uint32_t getUpper(void) {
	return md->upper;
}

void setUpper(uint32_t newU) {
	md->upper = newU;
}

uint32_t getLower(void) {
	return md->lower;
}

uint32_t getTransferSeqNum(void) {
	return md->seqNum;
}

void incrementTransferSeqNum(void) {
	md->seqNum = md->seqNum + 1;
}

int getNumItems(void) {
	return (int)numItems;
}

/* allocates memory for array of Frames of specified size and meta data */
MetaData* initQueue(uint32_t ssize) {
	int i;
	if (ssize < MAXWINDOW) {
		/* allocate memory for window */
		if ((Queue = malloc(ssize * sizeof(Frame))) == NULL) {
			perror("malloc 1");
			exit(-1);
		}
		for (i = 0; i < ssize; i++) {
			Queue[i].flag = -1;
		}
		/* create metaData struct */
		if((md = malloc(sizeof(MetaData))) == NULL) {
			perror("malloc 2");
			exit(-1);
		}
		numItems = 0;
		md->windowSize = ssize;
		md->seqNum = 0;
		md->lower = 0;
		md->upper = (md->lower + md->windowSize);
		md->current = 0;
		return md;
	}
	return NULL;
}

/* returns whole queue */
Frame* getWholeQueue(void) {
	return Queue;
}

/* frees circular queue struct */
void teardownQueue(void) {
	free(Queue);
	free(md);
}

/* adds Frame to circular queue based off of sequence number */
/* returns index Frames was placed in or -1 if payload was too large */
int addQueue(uint32_t seqNum, uint8_t* payload, int payloadLen) {
	int current = seqNum % (md->windowSize);
	if (payloadLen <= MAXFRAME) {
		memcpy(&(Queue[current].buff[0]), payload, payloadLen);
		Queue[current].seqNum = seqNum;
		Queue[current].frameLen = payloadLen;
		Queue[current].flag = 1;
		/* increment seqNum and current */
		md->seqNum = seqNum;
		md->current = md->current + 1;
		numItems++;
		return current;
	}
	return -1;
}

/* adds Frame to circular queue based off of sequence number */
/* returns index Frames was placed in or -1 if payload was too large */
int addRcopyQueue(uint32_t seqNum, uint8_t* payload, int payloadLen) {
	int current = seqNum % (md->windowSize);
	if (payloadLen <= MAXFRAME) {
		memcpy(&(Queue[current].buff[0]), payload, payloadLen);
		Queue[current].seqNum = seqNum;
		Queue[current].frameLen = payloadLen;
		Queue[current].flag = 1;
		numItems++;
		return current;
	}
	return -1;
}

/* checks to see if sequence number is in queue */
/* returns index if it is, returns -1 if it isn't */
int getQueue(uint32_t seqNum) {
	int index = seqNum % (md->windowSize);
	if (Queue[index].flag == -1) {
		return -1;
	}
	return Queue[index].seqNum;
}

/* is checking to see if specific seqNum exists at index */
/* returns index if data is good and seqNums match, else -1 */
int checkQueue(uint32_t seqNum) {
	int index = seqNum % (md->windowSize);
	// invalid data at index
	if (Queue[index].flag == -1) {
		return -1;
	}
	// seqNums don't match
	if (Queue[index].seqNum != seqNum) {
		return -1;
	}
	return index;
}

/* deletes entry if seqNum matches Frames sequence number at index */
/* "deleting" is changing seqNum and frameLen to -1 and zeroing frame */
/* returns index on success, -1 on failure */
void removeQueue(uint32_t seqNum) {
	uint32_t index = seqNum % md->windowSize;
	if (Queue[index].seqNum == seqNum) {
		memset(Queue[index].buff, 0, MAXFRAME);
		Queue[index].seqNum = -1;
		Queue[index].frameLen = 0;
		Queue[index].flag = -1;
		numItems--;
	}
}

/* cleans server queue when RR is received */
void updateServerQueue(uint32_t ack) {
	int i;
	md->lower = ack;
	md->upper = md->lower + md->windowSize;
	for (i = 0; i < md->lower; i++) {
		removeQueue(i);
	}
}

/* checks buffer for next frame after RR that can be sent */
Frame updateRcopyQueue(void) {
	int index;
	Frame error, good;
	error.seqNum = -1;
	// scan indices in buffer for frames that can be sent
	index = checkQueue(md->seqNum);
	// return error frame if not in order
	if (index == -1) {
		return error;
	}
	// if in order remove from queue and return
	good = getFrameData(md->seqNum);
	removeQueue(md->seqNum);
	// increment seqNum
	md->seqNum = md->seqNum + 1;
	return good;
}

Frame getFrameData(uint32_t seqNum) {
	Frame error;
	error.seqNum = -1;
	int index = seqNum % (md->windowSize);
	int err = getQueue(seqNum);
	/* check if valid entry */
	if (err == -1) {
		return error;
	}
	
	return Queue[index];
}

/* prints out window meta data */
/* window = 0, 1 output window data */
void printMD(void) {
	printf("Server Window - Window Size: %d, lower: %d, Upper: %d, Current: %d window open?: ", md->windowSize, md->lower, md->upper, md->current);
	if (md->current < md->upper) {
		printf("1\n");
	}
	else {
		printf("0\n");
	}
}

/* prints entire window */
void printWindow(void) {
	int i;
	printf("Window size is: %d\n", md->windowSize);
	for (i = 0; i < md->windowSize; i++) {
		if (Queue[i].flag == 1) {
			printf("	%d sequenceNumber: %d pduSize: %d\n", i, Queue[i].seqNum, Queue[i].frameLen);
		}	
		else {
			printf("	%d not valid\n", i);
		}
	}
}
