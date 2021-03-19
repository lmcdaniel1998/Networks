#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "networks.h"
#include "checksum.h"

#define FIRST 0xff000000
#define SECOND 0x00ff0000
#define THIRD 0x0000ff00
#define FOURTH 0x000000ff
#define MAXFILE 100

// sequenceNumber = 32 bit sequence number in network order
// flag = the type of the PDU
// payload = payload of the PDU may not be null terminated
// dataLen = lenght of the payload, used for memcpy

uint8_t* createPDU(uint32_t sequenceNumber, uint8_t flag, char* payload, int dataLen)
{
	static uint8_t pduBuffer[MAXFRAME];
	uint32_t networkSequenceNumber;
	// convert sequence number to network order and break apart
	networkSequenceNumber = htonl(sequenceNumber);
	memcpy(&pduBuffer[0], &networkSequenceNumber, 4);
	// place checksum into PDU
	// for lab no checksum
	pduBuffer[4] = 0x00;
	pduBuffer[5] = 0x00;

	// place flag into PDU
	pduBuffer[6] = flag;

	// memcpy payload into PDU
	if (dataLen != 0) {
		memcpy(&pduBuffer[HEADERLEN], payload, dataLen);
	}
	
	return pduBuffer;
}

void setChecksum(uint8_t* aPDU, int pduLength)
{
	uint16_t checksum = (uint16_t)in_cksum((unsigned short*)aPDU, pduLength);
	memcpy((aPDU + 4), &checksum, sizeof(checksum));
}

int checkCheckSum(uint8_t* aPDU, int pduLength)
{
	uint16_t checksum = (uint16_t)in_cksum((unsigned short*)aPDU, pduLength);
	if (checksum == 0) {
		return 0;
	}
	return -1;
}

uint32_t getSeqNum(uint8_t* aPDU, int pduLength)
{
	uint32_t networkSequenceNumber, hostSequenceNumber = 0x00000000;
	memcpy(&networkSequenceNumber, &aPDU[0], 4);
	hostSequenceNumber = ntohl(networkSequenceNumber);
	return hostSequenceNumber;
}

uint16_t getChecksum(uint8_t* aPDU, int pduLength)
{
	uint16_t checksum = 0x0000;
	memcpy(&checksum, &aPDU[4], 2);
	return checksum;
}

uint8_t getFlag(uint8_t* aPDU, int pduLength)
{
	uint8_t flag;
	flag = aPDU[6];
	return flag;
}

uint8_t* getPayload(uint8_t* aPDU, int pduLength)
{
	static uint8_t payload[MAXBUF];
	// print out data
	memcpy(&payload[0], (aPDU + HEADERLEN), pduLength - HEADERLEN);
	return payload;
}

int setFileName(uint32_t windowSize, uint32_t bufferSize, char* filename, uint8_t* payload)
{
	int payloadLen;
	// place window size and buffer size in payload
	memcpy(&payload[0], &windowSize, sizeof(windowSize));	
	memcpy(&payload[4], &bufferSize, sizeof(bufferSize));
	// place filename into payload include null terminator
	payloadLen = strlen(filename);
	memcpy(&payload[8], filename, payloadLen);
	payloadLen = payloadLen + 8;
	// return payload length
	return payloadLen;
}

void setRR(uint8_t* payload, uint32_t serverSeqNum) {
	uint32_t networkSQ = 0x00000000;
	networkSQ = htonl(serverSeqNum);
	memcpy(&payload[0], &networkSQ, sizeof(uint32_t));
}

void setSRJ(uint8_t* payload, uint32_t serverSeqNum) {
	uint32_t networkSQ = 0x00000000;
	networkSQ = htonl(serverSeqNum);
	memcpy(&payload[0], &networkSQ, sizeof(uint32_t));
}

uint32_t getRR(uint8_t* aPDU, int pduLength) {
	uint32_t nrr, hrr = 0x00000000;
	memcpy(&nrr, &aPDU[HEADERLEN], 4);
	hrr = ntohl(nrr);
	return hrr;
}

uint32_t getSRJ(uint8_t* aPDU, int pduLength) {
	uint32_t nsrj, hsrj = 0x00000000;
	memcpy(&nsrj, &aPDU[HEADERLEN], 4);
	hsrj = ntohl(nsrj);
	return hsrj;
}

uint32_t getWindowSize(uint8_t* aPDU, int pduLength)
{
	uint32_t windowSize = 0x00000000;
	memcpy(&windowSize, &aPDU[HEADERLEN], 4);
	return windowSize;
}

uint32_t getBufferSize(uint8_t* aPDU, int pduLength)
{
	uint32_t bufferSize = 0x00000000;
	memcpy(&bufferSize, &aPDU[HEADERLEN + 4], 4);
	return bufferSize;
}

void getFilename(uint8_t* aPDU, int pduLength, char* filename)
{
	int index = HEADERLEN + 8;
	int fileLength = pduLength - index;
	memcpy(filename, (aPDU + index), fileLength);
	filename[fileLength] = '\0';
}

void outputPDU(uint8_t* aPDU, int pduLength)
{
	uint32_t networkSequenceNumber, hostSequenceNumber = 0x00000000;
	uint16_t checksum = 0x0000;
	uint8_t flag;
	char payload[MAXFRAME];
	int i;
	// extract seqNum
	memcpy(&networkSequenceNumber, &aPDU[0], 4);
	hostSequenceNumber = ntohl(hostSequenceNumber);
	printf("sequence number host order: %x\n", hostSequenceNumber);

	// extract checksum
	memcpy(&checksum, &aPDU[4], 2);
	printf("checksum: %x\n", checksum);
	
	// extract flags
	flag = aPDU[6];
	printf("flags: %x\n", flag);

	// print out data
	if (pduLength != HEADERLEN) {
		memcpy(&payload[0], (aPDU + HEADERLEN), pduLength - HEADERLEN);
		printf("payload: ");
		for (i = 0; i < pduLength - HEADERLEN; i++) {
			printf("%x ", payload[i]);
		}
		printf("\n");
		//printf("payload: %.*s\n", (pduLength + HEADERLEN), payload);
		printf("dataLen: %d\n", (pduLength - HEADERLEN + 1));
	}
}
