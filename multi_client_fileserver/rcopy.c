/* File Transfer Client						*/
/* By: Luke McDaniel						*/
/* Intstitution: California Polytechnic State University	*/		
/* Last Edit: 03/01/2021					*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "cpe464.h"
#include "createPDU.h"
#include "rcopy.h"
#include "checksum.h"
#include "circularQueue.h"

#define xstr(a) str(a)
#define str(a) #a

/* define states for file transfer protocol */
typedef enum State STATE;
enum State
{
	START, DONE, INIT, WAIT_ON_INIT, FILENAME, 
	WAIT_ON_FILENAME, RECEIVE, SENDEOF 
};

/* declare all functions in file */
void talkToServer(int socketNum, struct sockaddr_in6 * server, uint32_t sequenceNumber);

int getData(char * buffer);

void checkArgs(int argc, char* argv[], struct inputArgs* rcopyArgs);

void downloadFile(struct sockaddr_in6* server, struct inputArgs* rcopyArgs);

STATE connectClient(int* socketNum, struct sockaddr_in6* server, struct inputArgs* rcopyArgs);

STATE sendInit(int* socketNum, struct sockaddr_in6* server, int* initNum);

STATE waitInit(int* socketNum, struct sockaddr_in6* server, int* counter, uint8_t* recvBuf, struct inputArgs* rcopyArgs);

STATE sendDownloadInfo(int* socketNum, struct sockaddr_in6* server, struct inputArgs* rcopyArgs);

STATE waitDownloadInfo(int* socketNum, struct sockaddr_in6* server, int* counter, uint8_t* recvBuf, int* initNum, struct inputArgs* rcopyArgs, int* downloadFD);

STATE recvData(int* socketNum, struct sockaddr_in6* server, int* counter, int* initNum, uint32_t* serverSeqNum, uint32_t* clientSeqNum, MetaData* md, struct inputArgs* rcopyArgs, int* downloadFD);

void sendRR(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md);

void sendSRJ(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md);

STATE returnEOF(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md);
/* end function declarations */

/* get arguments and initialize sendErr */
int main (int argc, char *argv[])
 {
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int SendErr;
	struct inputArgs* rcopyArgs;

	// create structure to store program arguments
	rcopyArgs = malloc(sizeof(struct inputArgs));
	checkArgs(argc, argv, rcopyArgs);
	// initialize sendErr
	SendErr = sendErr_init(rcopyArgs->errorPercent, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);
	
	downloadFile(&server, rcopyArgs);

	SendErr++;	
	close(socketNum);

	return 0;
}

/* manages state machine for file transfer process */
void downloadFile(struct sockaddr_in6* server, struct inputArgs* rcopyArgs)
{
	STATE state = START;
	int initNum = 0;
	int socketNum = 0;
	int counter = 0;
	static uint8_t recvBuf[MAXFRAME];
	uint32_t serverSeqNum = 0;
	uint32_t clientSeqNum = 0;
	int downloadFD = 0;
	MetaData *md = NULL;

	while (state != DONE) {
		switch (state) {
		case START:
			state = connectClient(&socketNum, server, rcopyArgs);
			break;

		case INIT:
			state = sendInit(&socketNum, server, &initNum);
			break;

		case WAIT_ON_INIT:
			state = waitInit(&socketNum, server, &counter, &recvBuf[0], rcopyArgs);
			break;

		case FILENAME:
			state = sendDownloadInfo(&socketNum, server, rcopyArgs);
			break;

		case WAIT_ON_FILENAME:
			state = waitDownloadInfo(&socketNum, server, &counter, &recvBuf[0], &initNum, rcopyArgs, &downloadFD);
			break;
		
		case RECEIVE:
			state = recvData(&socketNum, server, &counter, &initNum, &serverSeqNum, &clientSeqNum, md, rcopyArgs, &downloadFD);
			break;

		case SENDEOF:
			state = returnEOF(&socketNum, server, &initNum, &clientSeqNum, md);
			break;

		case DONE:
			close(downloadFD);	// close file
			teardownQueue();
			return;
		}
	}
}

/* START STATE */
/* connect to main server socket and go to init state */
STATE connectClient(int* socketNum, struct sockaddr_in6* server, struct inputArgs* rcopyArgs)
{
	/* connect to main server socket*/
	if ((*socketNum = setupUdpClientToServer(server, rcopyArgs->remoteMachine, rcopyArgs->portNumber)) < 0) {
		return DONE;
	}
	return INIT;
}

/* INIT STATE */
/* sends packet with flag = 1 to server and returns to WAIT_ON_INIT state */
/* if initNum is 1 then start file transfer with windowing */
STATE sendInit(int* socketNum, struct sockaddr_in6* server, int* initNum)
{
	int serverAddrLen = sizeof(struct sockaddr_in6);
	/* create initial packet */
	uint8_t* initPacket;
	initPacket = createPDU(0, 1, NULL, 0);
	// set checksum here
	setChecksum(initPacket, HEADERLEN);
	// send init packet
	sendtoErr(*socketNum, initPacket, HEADERLEN, 0, (struct sockaddr *) server, serverAddrLen);
	if (*initNum == 1) {
		return RECEIVE;
	}
	return WAIT_ON_INIT;
}

/* WAIT_ON_INIT STATE */
/* receives response from init packet and goes to filename transfer sequence */
/* will eventually connect client to child port number sent by the server */
STATE waitInit(int* socketNum, struct sockaddr_in6* server, int* counter, uint8_t* recvBuf, struct inputArgs* rcopyArgs)
{
	uint8_t flag;
	uint32_t dataLen;	
	uint8_t* forkPortPay;
	uint16_t forkPortN = 0x0000;
	uint16_t forkPort;
	int serverAddrLen = sizeof(struct sockaddr_in6);
	
	// wait 10 times before quitting
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		if (*counter < 9) {
			// reset counter and resend flag = 1 packet
			(*counter)++;
			return INIT;
		}
		else {
			printf("timeout after 10 seconds, server is gone.\n");
			return DONE;
		}
	}
	// recv data
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) server, &serverAddrLen);
	// do checksum calculation here
	if (checkCheckSum(recvBuf, dataLen) != 0) {
		(*counter) = 0;
		return INIT;
	}
	// get flag, should be 2
	flag = getFlag(recvBuf, dataLen);
	// if flag is not 2 or no data resend init packet
	if (flag != 2 || dataLen < (HEADERLEN + 2)) {
		(*counter) = 0;
		return INIT;
	} 
	// send filename packet if flag = 2
	// get fork port number from packet
	forkPortPay = getPayload(recvBuf, dataLen);
	memcpy(&forkPortN, &forkPortPay[0], 2);
	forkPort = ntohs(forkPortN);
	rcopyArgs->portNumber = forkPort;
	// close old socket
	close(*socketNum);
	// open new socket to fork
	if ((*socketNum = setupUdpClientToServer(server, rcopyArgs->remoteMachine, rcopyArgs->portNumber)) < 0) {
		return DONE;
	}
	server->sin6_port = forkPort;
	// send filename on new socket
	(*counter) = 0;
	return FILENAME;
}

/* FILENAME STATE */
/* sends packet flag = 7 and filename to server for verification */
STATE sendDownloadInfo(int* socketNum, struct sockaddr_in6* server, struct inputArgs* rcopyArgs)
{
	uint8_t payload[MAXBUF];
	int payloadLen;
	uint8_t* sendBuf;
	int serverAddrLen = sizeof(struct sockaddr_in6);
	// create payload windowSize, bufferSize, filename
	payloadLen = setFileName(rcopyArgs->windowSize, rcopyArgs->bufferSize, &(rcopyArgs->fromFile[0]), &payload[0]);
	// create PDU
	sendBuf = createPDU(0, 7, (char*)(&payload[0]), payloadLen);
	// set checksum here
	setChecksum(sendBuf, (HEADERLEN + payloadLen));
	// send to server
	sendtoErr(*socketNum, sendBuf, HEADERLEN + payloadLen, 0, (struct sockaddr *) server, serverAddrLen);
	return WAIT_ON_FILENAME;
}

/* WAIT_ON_FILENAME STATE */
/* receives response from filename packet */
/* if file is on server and can be created/opened by client, send flag = 1 packet to start data transfer */
STATE waitDownloadInfo(int* socketNum, struct sockaddr_in6* server, int* counter, uint8_t* recvBuf, int* initNum, struct inputArgs* rcopyArgs, int* downloadFD) 
{
	uint8_t flag;
	uint32_t dataLen;
	uint8_t* payload;
	int payloadLen;
	char ok[3] = { 'O', 'K', '\0'};
	int serverAddrLen = sizeof(struct sockaddr_in6);
	// send 10 times before quitting
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		if ((*counter) < 9) {
			// reset counter and resend filename packet
			(*counter)++;
			return FILENAME;
		}
		else {
			printf("timeout after 10 seconds, server is gone.\n");
			return DONE;
		}
	}
	// recv data
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) server, &serverAddrLen);
	// do checksum calculation here
	if (checkCheckSum(recvBuf, dataLen) != 0) {
		(*counter) = 0;
		return FILENAME;
	}
	// get flag, should be 8
	flag = getFlag(recvBuf, dataLen);
	// if flag != 8 resend packet
	if (flag != 8) {
		(*counter) = 0;
		return FILENAME;
	} 
	payload = getPayload(recvBuf, dataLen);
	payloadLen = dataLen - HEADERLEN;
	payload[payloadLen + 1] = '\0';
	// check if file is good on server
	if (strcmp(&ok[0], (char*)payload) == 0) {
		// if file is good, open file and send packet flag = 1 again to server to let it know to start sending data
		(*initNum) = 1;
		(*counter) = 0;
		// open and create new file
		if (((*downloadFD) = open((rcopyArgs->toFile), O_CREAT | O_RDWR, 0666)) < 0) {
			perror("open");
			return DONE;
		}
		return INIT;
	}
	// else file is not on server
	printf("file: %s not on server.\n", &payload[0]);
	(*counter) = 0;
	return DONE;
}

/* RECEIVE STATE */
/* this is the primary state for rcopy, it receives data, writes to the file and sends rr/srejs */
STATE recvData(int* socketNum, struct sockaddr_in6* server, int* counter, int* initNum, uint32_t* serverSeqNum, uint32_t* clientSeqNum, MetaData* md, struct inputArgs* rcopyArgs, int* downloadFD) 
{
	static uint8_t recvBuf[MAXFRAME];
	int dataLen;
	uint8_t* payload;
	uint8_t flag, eofFlag;
	Frame temp;
	int serverAddrLen = sizeof(struct sockaddr_in6);

	// first time being accessed initialize buffer
	if (*initNum == 1 && *counter == 0) {
		md = initQueue(rcopyArgs->windowSize);
		setUpper(0);
	}
	// timeout after 10 seconds means server has terminated exit
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		if ((*counter) < 9) {
			(*counter)++;
			// haven't received any data from server so resend init
			if (*initNum == 1) {
				return INIT;
			}
			return RECEIVE;
		}
		else {
			printf("timeout after 10 seconds, server is gone.\n");
			return DONE;
		}
	}
	// receive packet from server
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) server, &serverAddrLen);
	(*initNum) = 2;
	(*counter) = 0;
	// do checksum calculation here
	if (checkCheckSum(recvBuf, dataLen) != 0) {
		return RECEIVE;
	}
	flag = getFlag(recvBuf, dataLen);
	// flag = 3 data packet
	if (flag == 3) {
		// get packet seqNum
		(*serverSeqNum) = getSeqNum(recvBuf, dataLen);
		// set upper
		if (getUpper() < (*serverSeqNum)) {
			setUpper(*serverSeqNum);
		}
		// in proper order
		if (getTransferSeqNum() == *serverSeqNum) {
			// write packet data to file
			payload = getPayload(recvBuf, dataLen);
			if (write(*downloadFD, payload, (dataLen - HEADERLEN)) < 0) {
				perror("write");
				return DONE;
			}
			// increment buffer
			incrementTransferSeqNum();
			temp = updateRcopyQueue();
			// if sequence number is up to date or there is data in next buffer spot send RR
			//if (getUpper() > (*serverSeqNum) && temp.seqNum != getTransferSeqNum()) {
			if (getUpper() > (*serverSeqNum) && temp.seqNum == -1) {
				// send srej
				sendSRJ(socketNum, server, initNum, clientSeqNum, md);
			}
			else {
				// send RR for packet received
				sendRR(socketNum, server, initNum, clientSeqNum, md);
			}
			// check buffer for subsequent packets already received
			while (temp.seqNum != -1) {
				eofFlag = getFlag(temp.buff, temp.frameLen);
				// normal packet pulled, write to file then rr
				if (eofFlag != 9) {
					// write packet data to file
					payload = getPayload(temp.buff, temp.frameLen);
					if (write(*downloadFD, payload, (temp.frameLen - HEADERLEN)) < 0) {
						perror("write");
						return DONE;
					}
					// send RR for md->seqNum packet
					sendRR(socketNum, server, initNum, clientSeqNum, md);		
				}
				// eof packet pulled
				else {
					close(*downloadFD);
					return SENDEOF;
				}
				temp = updateRcopyQueue();		
			}
		}
		// bad order packet lower than seqNum
		else if (getTransferSeqNum() > *serverSeqNum) {
			// "throw out" packet
			// send RR for in order seqNum (md->seqNum)
			sendRR(socketNum, server, initNum, clientSeqNum, md);
			 
		}
		// bad order packet higher than seqNum
		else {
			// place out of order packet in buffer
			if (getQueue(*serverSeqNum) == -1) {
				if (addRcopyQueue(*serverSeqNum, recvBuf, dataLen) == -1) {
					printf("packet data is too large\n");
				}
				// send SREJ for md->seqNum
				if (getTransferSeqNum() + 1 == *serverSeqNum) {
					sendSRJ(socketNum, server, initNum, clientSeqNum, md);
				}
			}
			else {
				printf("index occupied could not be buffered\n");
			}
		}
	}
	// received EOF packet, send back to server
	if (flag == 9) {
		// if buffer is empty (no more data needs to come in from server) shutdown
		if (getNumItems() == 0) {
			close((*downloadFD));
			return SENDEOF;
		}
		// buffer is not empty add EOF to buffer and continue
		if (getQueue(*serverSeqNum) == -1) {
			if (addRcopyQueue(*serverSeqNum, recvBuf, dataLen) == -1) {
				printf("EOF packet too large, bad news\n");
				// no need to SREJ EOF, just dump from buffer when reached
			}
		}
		else {
			printf("index occupied could not be buffered\n");
		}
	}
	return RECEIVE;
}

/* this function sends out an RR for the specified packet */
void sendRR(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md)
{
	uint8_t payload[MAXBUF];
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t* rrPacket;
	// create rr frame
	setRR(&payload[0], getTransferSeqNum());
	rrPacket = createPDU(*clientSeqNum, 5, (char*)&payload[0], 4);
	// set checksum here
	setChecksum(rrPacket, HEADERLEN + 4);
	// send init packet
	sendtoErr(*sockNum, rrPacket, HEADERLEN + 4, 0, (struct sockaddr *) server, serverAddrLen);
	(*clientSeqNum)++;
}

/* this function sends out an SREJ for the specified packet */
void sendSRJ(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md)
{
	uint8_t payload[MAXBUF];
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t* srjPacket;
	// create srej frame
	setSRJ(&payload[0], getTransferSeqNum());
	srjPacket = createPDU(*clientSeqNum, 6, (char*)&payload[0], 4);
	// set checksum here
	setChecksum(srjPacket, HEADERLEN + 4);
	// send init packet
	sendtoErr(*sockNum, srjPacket, HEADERLEN + 4, 0, (struct sockaddr *) server, serverAddrLen);
	(*clientSeqNum)++;
}

/* sends response packt for EOF then exits client */
STATE returnEOF(int* sockNum, struct sockaddr_in6* server, int* initNum, uint32_t* clientSeqNum, MetaData* md)
{
	int serverAddrLen = sizeof(struct sockaddr_in6);
	uint8_t* eofRRPacket;
	// flag = 10 is RR for EOF
	eofRRPacket = createPDU(*clientSeqNum, 10, NULL, 0);
	// set checksum here
	setChecksum(eofRRPacket, HEADERLEN);
	// send init packet
	sendtoErr(*sockNum, eofRRPacket, HEADERLEN, 0, (struct sockaddr *) server, serverAddrLen);
	// wrap up client after this, no response expected from server
	return DONE;
}

/* checks argument format and creates structure to store input parameters */
void checkArgs(int argc, char* argv[], struct inputArgs* rcopyArgs)
{	
	int stringLength;
	char* ptr;
	/* check correct number of args */
	if (argc != 8) {
		fprintf(stderr, "rcopy takes 7 arguments in the form: from-filename to-filename window-size buffer-size error-percent remote-machine remote-port");
			exit(-1);
	}
	/* get from-filename */
	if ((stringLength = strlen(argv[1])) <= MAXFILELEN) {
		memcpy(&(rcopyArgs->fromFile[0]), argv[1], stringLength);
	}
	else {
		fprintf(stderr, "from file is too long: max 100 characters");
		free(rcopyArgs);
		exit(-1);
	}
	/* get to-filename */
	if ((stringLength = strlen(argv[2])) <= MAXFILELEN) {
		memcpy(&(rcopyArgs->toFile[0]), argv[2], stringLength);
	}
	else {
		fprintf(stderr, "to file is too long: max 100 characters");
		free(rcopyArgs);
		exit(-1);
	}
	/* get window size */
	rcopyArgs->windowSize = ((uint32_t)(atoi(argv[3])));
	/* get buffer size */
	rcopyArgs->bufferSize = ((uint32_t)(atoi(argv[4])));
	/* get error percent */
	rcopyArgs->errorPercent = strtod(argv[5], &ptr);
	/* get remote machine */
	if ((stringLength = strlen(argv[6])) <= 100) {
		memcpy(&(rcopyArgs->remoteMachine[0]), argv[6], stringLength);
	}
	else {
		fprintf(stderr, "remote machine is too long: max 100 characters");
		free(rcopyArgs);
		exit(-1);
	}
	/* get port number */
	rcopyArgs->portNumber = atoi(argv[7]);
	return;
}
