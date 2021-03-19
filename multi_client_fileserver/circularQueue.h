#include <stdlib.h>
#include <stdint.h>

#define MAXPAYLOAD 1400
#define MAXFRAME 1407
#define MAXWINDOW 1073741824	/* 2^30 is max window */

/* single frame with PDU */
typedef struct Frame{
	uint8_t buff[MAXFRAME];
	uint16_t frameLen;
	uint32_t seqNum;
	int flag;
}Frame;

/* contains window meta data */
typedef struct MetaData {
	uint32_t windowSize;
	uint32_t seqNum;
	uint32_t lower;
	uint32_t upper;		// used by rcopy to store highest received packet from server
	uint32_t current;
}MetaData;

uint32_t getCurrent(void);

uint32_t getUpper(void);

void setUpper(uint32_t newU);

uint32_t getLower(void);

uint32_t getTransferSeqNum(void);

void incrementTransferSeqNum(void);

int getNumItems(void);

MetaData* initQueue(uint32_t ssize);

Frame* getWholeQueue(void);

void teardownQueue(void);

int addQueue(uint32_t seqNum, uint8_t* payload, int payloadLen);

int addRcopyQueue(uint32_t seqNum, uint8_t* payload, int payloadLen);

int getQueue(uint32_t seqNum);

int checkQueue(uint32_t seqNum);

void removeQueue(uint32_t seqNum);

void updateServerQueue(uint32_t ack);

Frame updateRcopyQueue(void);

Frame getFrameData(uint32_t seqNum);

void printMD(void);

void printWindow(void);

Frame getFrameData(uint32_t seqNum);
