/* File Transfer Server						*/
/* By: Luke McDaniel						*/
/* Intstitution: California Polytechnic State University	*/		
/* Last Edit: 03/01/2021					*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "gethostbyname.h"
#include "networks.h"
#include "cpe464.h"
#include "createPDU.h"
#include "server.h"
#include "circularQueue.h"

#define MAXFILE 100

/* define states for file transfer state machine */
typedef enum State STATE;
enum State
{
	DONE, START, WAIT_ON_INIT, SEND_INIT, WAIT_FILENAME, FILE_OPEN_ERROR,
	FILE_OPEN_OK, OPEN_WINDOW, CLOSE_WINDOW, SENDEOF
};

/* declare all function in file */
void processClient(int socketNum);

void checkArgs(int argc, char* argv[], struct argsInput* serverArgs);

void serverProcess(int socketNum);

void sendFile(int serverSocketNum, uint8_t* buffer, int recvLen, struct sockaddr_in6* client);

STATE newSocket(int* forkSocketNum, struct sockaddr_in6* forkInfo);

STATE waitInit(int* socketNum, struct sockaddr_in6* client, int* counter, uint8_t* recvBuf, int* initNum);

STATE sendInit(int* serverSocketNum, int* forkSocketNum, struct sockaddr_in6* client, struct sockaddr_in6* forkInfo, int* initNum);

STATE waitDownloadInfo(int* socketNum, struct sockaddr_in6* client, int* counter, uint8_t* recvBuf, int* initNum, uint32_t* wSz, uint32_t* bSz, int* transferFD, char* fileName);

STATE fileOpenError(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, char* fileName);

STATE fileOpenOK(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter);

STATE openWindow(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, uint32_t* seqNum, int* transferFD, uint32_t* bufferSize, uint32_t* windowSize, MetaData* md);

STATE closedWindow(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, uint32_t* seqNum, uint32_t* bufferSize, uint32_t* windowSize, MetaData* md, int* transferFD);

STATE sendEOF(int* socketNum, struct sockaddr_in6* client, int* counter, int* initNum, uint32_t* seqNum, uint32_t* bufferSize, MetaData *md, int* transferFD);

int sendSREJ(int* socketNum, struct sockaddr_in6* client, uint32_t* bufferSize, uint32_t srej, MetaData *md, int firstCall, int* srejCounter, int* transferFD);

/* end function declarations */

/* gets arguments, initializes server socket, initializes sendErr */
int main ( int argc, char *argv[]  )
{ 
	int socketNum = 0;				
	struct argsInput* serverArgs;
	int SendErr;
	// create structure to store program arguments
	serverArgs = malloc(sizeof(struct argsInput));
	// get program arguments
	checkArgs(argc, argv, serverArgs);
	// call create sendtoErr struct
	SendErr = sendErr_init(serverArgs->errorPercent, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_OFF);
	// set up main server socket
	socketNum = udpServerSetup(serverArgs->portNumber);

	serverProcess(socketNum);
	
	// clean shutdown
	SendErr++;
	close(socketNum);
	return 0;
}

/* wait for incomming client connections */
/* forks child processes for each client */
void serverProcess(int socketNum)
{
	pid_t pid = 0;
	int status = 0;
	static uint8_t buf[MAXFRAME];
	struct sockaddr_in6 client;
	int recvLen = 0;
	uint8_t flag = 0;
	int clientAddrLen = sizeof(struct sockaddr_in6);
	
	// get new client and create child with new socket
	while(1) {
		// wait for new client
		if (selectCall(socketNum, 10, 0, TIME_IS_NOT_NULL) == 1) {
			// receive setup packet
			recvLen = safeRecvfrom(socketNum, &buf[0], MAXFRAME, 0, (struct sockaddr*) &client, &clientAddrLen);
			// do checksum calculation
			if (checkCheckSum(&buf[0], recvLen) == 0) {
				// check flags, must be 1
				if((flag = getFlag(&buf[0], recvLen)) == 1) {
					// fork child process
					if ((pid = fork()) < 0) {
						perror("fork");
						exit(-1);
					}
					if (pid == 0) {
						// child process (file transfer for one rcopy client)
						sendFile(socketNum, buf, recvLen, &client);
						exit(EXIT_SUCCESS);
					}
				}
			}
		}
		while (waitpid(-1, &status, WNOHANG) > 0);
	}
}

/* manages state machine for server file transfer process */
void sendFile(int serverSocketNum, uint8_t* buffer, int recvLen, struct sockaddr_in6* client)
{
	STATE state = START;
	int initNum = 0;	// used for setup and filename exchange
	int socketNum = serverSocketNum;
	int forkSocketNum = 0;
	struct sockaddr_in6 forkInfo;
	int counter = 0;
	uint32_t windowSize = 0;
	uint32_t bufferSize = 0;
	uint32_t seqNum = 0;
	int transferFD = 0;
	static char fileName[MAXFILE + 1];
	MetaData *md = NULL;
	static uint8_t recvBuf[MAXFRAME];

	while (state != DONE) {
		switch (state) {
		case START:
			state = newSocket(&forkSocketNum, &forkInfo);
			break;
		
		case WAIT_ON_INIT:
			state = waitInit(&forkSocketNum, &forkInfo, &counter, &recvBuf[0], &initNum);
			break;

		case SEND_INIT:
			state = sendInit(&socketNum, &forkSocketNum, client, &forkInfo, &initNum);
			break;

		case WAIT_FILENAME:
			state = waitDownloadInfo(&forkSocketNum, &forkInfo, &counter, &recvBuf[0], &initNum, &windowSize, &bufferSize, &transferFD, &fileName[0]);
			break;

		case FILE_OPEN_OK:
			state = fileOpenOK(&forkSocketNum, &forkInfo, &initNum, &counter);
			break;

		case FILE_OPEN_ERROR:
			state = fileOpenError(&forkSocketNum, &forkInfo, &initNum, &counter, &fileName[0]);
			break;

		case OPEN_WINDOW:
			state = openWindow(&forkSocketNum, &forkInfo, &initNum, &counter, &seqNum, &transferFD, &bufferSize, &windowSize, md);
			break;

		case CLOSE_WINDOW:
			state = closedWindow(&forkSocketNum, &forkInfo, &initNum, &counter, &seqNum, &bufferSize, &windowSize, md, &transferFD);
			break;
		
		case SENDEOF:
			state = sendEOF(&forkSocketNum, &forkInfo, &counter, &initNum, &seqNum, &bufferSize, md, &transferFD);
			break;

		case DONE:
			close(transferFD);
			teardownQueue();
			close(forkSocketNum);
			return;
		}
	}
}

/* START STATE */
/* server has received packet with flag = 1 on main server socket */
/* go to SEND_IT to respond */
/* creates a new socket for the child process */
STATE newSocket(int* forkSocketNum, struct sockaddr_in6* forkInfo)
{
	// set up new socket for forked process
	if (((*forkSocketNum) = udpForkSetup(forkInfo)) < 0) {
		return DONE;
	}
	return SEND_INIT;
}

/* WAIT_ON_INIT STATE */
/* handles select calls and packet receiveing for connection and filename transfer sequences */
STATE waitInit(int* socketNum, struct sockaddr_in6* client, int* counter, uint8_t* recvBuf, int* initNum)
{
	uint8_t flag;
	uint32_t dataLen;
	int clientAddrLen = sizeof(struct sockaddr_in6);
	// wait 10 times before quitting
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		if ((*counter) < 9) {
			(*counter)++;
			// redirects state machine to correct state for filename transfer sequence
			if ((*initNum) == 3) {
				return FILE_OPEN_ERROR;
			}
			if ((*initNum) == 4) {
				return FILE_OPEN_OK;
			}
		}
		else {
			printf("timeout after 10 seconds, client is gone.\n");
			return DONE;
		}
	}
	// recv data
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) client, &clientAddrLen);
	// do checksum claculation here
	if (checkCheckSum(recvBuf, dataLen) != 0) {
		(*counter) = 0;
		return WAIT_ON_INIT;
	}
	// received ack for bad file packet exit fork
	if ((*initNum) == 3) {
		return DONE;
	}
	// get flag, should be 1
	flag = getFlag(recvBuf, dataLen);
	// if flag in not 1, resend file info
	if (flag != 1) {
		if ((*initNum == 4)) {
			(*initNum) = 4;
			(*counter) = 0;
			return FILE_OPEN_OK;
		}
	}
	// received flag = 1 so go to windowing
	(*counter) = 0;
	return OPEN_WINDOW;
}

/* SEND_INIT STATE */
/* responds to client on server socket to estblish new socket for client to connect to */
/* sent packet has flag = 2 and contains port number for forked process socket as data */
/* goes to WAIT_FILENAME state to await response from client on new socket */
STATE sendInit(int* serverSocketNum, int* forkSocketNum, struct sockaddr_in6* client, struct sockaddr_in6* forkInfo, int* initNum)
{
	int AddrLen = sizeof(struct sockaddr_in6);
	uint8_t* initPacket;
	uint16_t hostforkPortNumber, networkforkPortNumber;
	static uint8_t initPayload[MAXBUF];
	// send packet with fork socket port number on server socket
	// init num must be zero because this is first step
	if ((*initNum) == 0) {
		printIPInfo(forkInfo);
		// port number will be in network order
		hostforkPortNumber = forkInfo->sin6_port;
		networkforkPortNumber = htons(hostforkPortNumber);
		memcpy(&initPayload[0], &networkforkPortNumber, 2);
		initPacket = createPDU(1, 2, (char*)(&initPayload[0]), 2);
		// set checksum here
		setChecksum(initPacket, HEADERLEN + 2);

		sendtoErr(*serverSocketNum, initPacket, HEADERLEN + 2, 0, (struct sockaddr *) client, AddrLen);
	}
	return WAIT_FILENAME;
}

/* WAIT_FILENAME STATE */
/* receives filename packet that contains, window size, buffer size, and filename */
/* if file opened successfully send file ok packet */
/* if file does not exist send file error packet */
/* if bad checksum or timeout, resend packet flag = 2 */
STATE waitDownloadInfo(int* socketNum, struct sockaddr_in6* client, int* counter, uint8_t* recvBuf, int* initNum, uint32_t* wSz, uint32_t* bSz, int* transferFD, char* fileName)
{
	uint8_t flag;
	uint32_t dataLen;
	int clientAddrLen = sizeof(struct sockaddr_in6);
	// wait 10 times before quitting
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		// on timeout increment counter, reset initNum to 0 and resend packet flag = 2
		if ((*counter) < 9) {
			(*counter)++;
			(*initNum) = 0;
			return SEND_INIT;
		}
		else {
			printf("timeout after 10 seconds, client is gone.\n");
			return DONE;
		}
	}
	// recv data
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) client, &clientAddrLen);
	// do checksum calculation here
	if (checkCheckSum(recvBuf, dataLen) != 0) {
		return WAIT_FILENAME;
	}
	// get flag, should be 7
	flag = getFlag(recvBuf, dataLen);
	// if flag != 7, reset counter and initNum then and resend packet flag = 2
	if (flag != 7) {
		(*initNum) = 0;
		(*counter) = 0;
		return SEND_INIT;
	}
	// extract data window size, buffer size, filename
	*wSz = getWindowSize(recvBuf, dataLen);
	*bSz = getBufferSize(recvBuf, dataLen);
	getFilename(recvBuf, dataLen, fileName);
	// try to open file
	if (((*transferFD) = open(fileName, O_RDONLY)) < 0) {
		// send file open error packet
		(*counter) = 0;
		return FILE_OPEN_ERROR;
	}
	// file was opened send file open packet
	(*counter) = 0;
	return FILE_OPEN_OK;
}

/* FILE_OPEN_ERROR STATE */
/* file could not be opened on server so, send packet flag = 8 and filename requested back */
STATE fileOpenError(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, char* fileName)
{
	int clientAddrLen = sizeof(struct sockaddr_in6);
	int fileLen = strlen(fileName);
	/* create filename error response packet */
	uint8_t* errPacket;
	errPacket = createPDU(0, 8, fileName, fileLen - 1);
	// set checksum here
	setChecksum(errPacket, (HEADERLEN + fileLen - 1));
	sendtoErr(*socketNum, errPacket, HEADERLEN + (fileLen - 1), 0, (struct sockaddr *) client, clientAddrLen);
	// initNum = 3 tells INIT_WAIT state to wait for ack on error packet then shutdown child
	(*initNum) = 3;	
	return WAIT_ON_INIT;
}

/* FILE_OPEN_OK STATE */
/* file was opened so, send packet flag = 8 and OK as data to let client know it should get ready to receive data */
STATE fileOpenOK(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter)
{
	int clientAddrLen = sizeof(struct sockaddr_in6);
	static char ok[2] = { 'O', 'K' };
	uint8_t* okPacket;
	okPacket = createPDU(2, 8, &ok[0], 2);
	// set checksum here
	setChecksum(okPacket, HEADERLEN + 2);
	sendtoErr(*socketNum, okPacket, HEADERLEN + 2, 0, (struct sockaddr *) client, clientAddrLen);
	// initNum = 4 tells INIT_WAIT state to wait for ack on ok packet then proceed to file transfer with windowing
	(*initNum) = 4;
	return WAIT_ON_INIT;
}

/* OPEN_WINDOW STATE */
/* sends all data in window then goes to window closed */
/* eventually when EOF is read go to EOF transfer sequence */
STATE openWindow(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, uint32_t* seqNum, int* transferFD, uint32_t* bufferSize, uint32_t* windowSize, MetaData* md)
{
	static uint8_t readBuf[MAXBUF];
	static uint8_t recvBuf[MAXFRAME];
	uint8_t* sendBuf;
	int readLen = 0;
	int dataLen;
	int packetLen = 0;
	uint8_t flag;
	int queueIndex;
	uint32_t rr, srej;
	int srejCounter = 0;
	int srejResult = 0;
	int clientAddrLen = sizeof(struct sockaddr_in6);

	// on first call, initialize window
	if (*seqNum == 0) {
		md = initQueue(*windowSize);
	}
	//printf("window open\n");
	// send data until window closes
	while (getCurrent() < getUpper()) {
		// read from file
		readLen = read(*transferFD, &readBuf[0], *bufferSize);
		// read error
		if (readLen == -1) {
			perror("read");
			return DONE;
		}
		// good read not EOF
		else {
			// send data packet
			if (readLen != 0) {
				// create packet
				sendBuf = createPDU(*seqNum, 3, (char*)(&readBuf[0]), readLen);
				packetLen = HEADERLEN + readLen;
				// set checksum here
				setChecksum(sendBuf, packetLen);
				// place in buffer
				queueIndex = addQueue(*seqNum, sendBuf, packetLen);
				if (queueIndex < 0) {
					printf("packet does not fit in queue\n");
					return DONE;
				}
				// send packet
				sendtoErr(*socketNum, sendBuf, HEADERLEN + readLen, 0, (struct sockaddr *) client, clientAddrLen);
				// increment seqNum
				(*seqNum)++;
			}
			// if no more data send EOF packet
			else {
				(*counter) = 0;
				(*initNum) = 1;
				return SENDEOF;
			}
		}
		// call select with no wait time
		if (selectCall(*socketNum, 0, 0, TIME_IS_NOT_NULL) != 0) {
			dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) client, &clientAddrLen);
			// do checksum calculation here
			if (checkCheckSum(recvBuf, dataLen) == 0) {
				// check flags to see if RR or SRJ
				flag = getFlag(recvBuf, dataLen);
				// RR = open window more
				if (flag == 5) {
					// get RR number from packet payload
					rr = getRR(recvBuf, dataLen);
					// slide window
					updateServerQueue(rr);
				}
				// SREJ = resend packet specified by SREJ
				if (flag == 6) {
					// get srej number
					srej = getSRJ(recvBuf, dataLen);
					// start srej recovery sub state machine
					srejResult = sendSREJ(socketNum, client, bufferSize, srej, md, 0, &srejCounter, transferFD);
					// if this occurs connection with client lost or serious error
					if (srejResult == 1) {
						return DONE;
					}
				}
				// EOF response
				if (flag == 10) {
					return DONE;
				}
			}
		}
	}
	(*counter) = 0;
	return CLOSE_WINDOW;
}

/* CLOSE_WINDOW STATE */
/* this state waits for responses from rcopy, in the forms of RRs and SREJs */
/* process responses and open window accordingly */
STATE closedWindow(int* socketNum, struct sockaddr_in6* client, int* initNum, int* counter, uint32_t* seqNum, uint32_t* bufferSize, uint32_t* windowSize, MetaData* md, int* transferFD)
{
	static uint8_t recvBuf[MAXFRAME];
	int dataLen;
	uint8_t flag;
	uint32_t rr, srej;
	int srejCounter = 0;
	int srejResult = 0;
	Frame lowestFrame;
	int clientAddrLen = sizeof(struct sockaddr_in6);
	// wait 10 times before quitting
	if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
		if ((*counter) < 9) {
			(*counter)++;
			// to prevent deadlock send lowest packet in window
			// pull lowest packet from buffer
			lowestFrame = getFrameData(getLower());
			// check if valid entry
			if (lowestFrame.seqNum == -1) {
				// honestly don't know what to do if no the right frame
			}
			// send lowest packet
			sendtoErr(*socketNum, lowestFrame.buff, lowestFrame.frameLen, 0, (struct sockaddr *) client, clientAddrLen);
			return CLOSE_WINDOW;	
		}
		else {
			printf("timeout after 10 seconds, client is gone.\n");
			return DONE;
		}
	}
	// recv data
	dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) client, &clientAddrLen);
	// do checksum claculation here
	if (checkCheckSum(recvBuf, dataLen) == 0) {
		// check flags to see if RR or SRJ
		flag = getFlag(recvBuf, dataLen);
		// RR = open window more
		if (flag == 5) {
			// get RR number from packet payload
			rr = getRR(recvBuf, dataLen);
			// slide window
			updateServerQueue(rr);
		}
		// SREJ = resend packet specified by SREJ
		if (flag == 6) {
			// get srej number
			srej = getSRJ(recvBuf, dataLen);
			// start srej recovery sub state machine
			srejResult = sendSREJ(socketNum, client, bufferSize, srej, md, 0, &srejCounter, transferFD);
			// if this occurs connection with client lost or serious error
			if (srejResult == 1) {
				return DONE;
			}
		}
		// EOF response
		if (flag == 10) {
			return DONE;
		}
	}
	return OPEN_WINDOW;	
}

/* SENDEOF STATE */
/* sends EOF packet then cleans up buffer with client */
STATE sendEOF(int* socketNum, struct sockaddr_in6* client, int* counter, int* initNum, uint32_t* seqNum, uint32_t* bufferSize, MetaData *md, int* transferFD)
{
	static uint8_t recvBuf[MAXFRAME];
	uint8_t* sendBuf;
	int packetLen = 0;
	int dataLen;
	uint8_t flag;
	uint32_t rr, srej;
	int srejCounter = 0;
	int srejResult = 0;
	int queueIndex;
	Frame lowestFrame;
	int clientAddrLen = sizeof(struct sockaddr_in6);

	// stay in this state unitl timeout or buffer has been drained
	while (getNumItems() > 0) {
		// only create EOF packet once
		if (*initNum != 10) {
			// create empty packet with flag = 9 EOF packet
			sendBuf = createPDU(*seqNum, 9, NULL, 0);
			packetLen = HEADERLEN;
			// set checksum here
			setChecksum(sendBuf, packetLen);
			// place in buffer
			queueIndex = addQueue(*seqNum, sendBuf, packetLen);
			if (queueIndex < 0) {
				printf("packet does not fit in queue\n");
				return DONE;
			}
			// send packet
			sendtoErr(*socketNum, sendBuf, packetLen, 0, (struct sockaddr *) client, clientAddrLen);
			// increment seqNum
			(*seqNum)++;
			// set initNum to 10 so this packet doesn't get sent again
			(*initNum) = 10;
		}
		// wait 10 times before quitting
		if (selectCall(*socketNum, 1, 0, TIME_IS_NOT_NULL) == 0) {
			if ((*counter) < 9) {
				(*counter)++;
				// to prevent deadlock send lowest packet in window
				// pull lowest packet from buffer
				lowestFrame = getFrameData(getLower());
				// check if valid entry
				if (lowestFrame.seqNum == -1) {
					// honestly don't know what to do if no the right frame
				}
				// send lowest packet
				sendtoErr(*socketNum, lowestFrame.buff, lowestFrame.frameLen, 0, (struct sockaddr *) client, clientAddrLen);
				return SENDEOF;	
			}
			else {
				printf("timeout after 10 seconds, client is gone.\n");
				return DONE;
			}
		}
		// recv data
		dataLen = safeRecvfrom(*socketNum, recvBuf, MAXFRAME, 0, (struct sockaddr*) client, &clientAddrLen);
		// do checksum claculation here
		if (checkCheckSum(recvBuf, dataLen) == 0) {
			// check flags to see if RR or SRJ
			flag = getFlag(recvBuf, dataLen);
			// RR = open window more
			if (flag == 5) {
				// get RR number from packet payload
				rr = getRR(recvBuf, dataLen);
				// slide window
				updateServerQueue(rr);
			}
			// SREJ = resend packet specified by SREJ
			if (flag == 6) {
				// get srej number
				srej = getSRJ(recvBuf, dataLen);
				// start srej recovery sub state machine
				srejResult = sendSREJ(socketNum, client, bufferSize, srej, md, 0, &srejCounter, transferFD);
				// if this occurs connection with client lost or serious error
				if (srejResult == 1) {
					return DONE;
				}
			}
			// EOF response
			if (flag == 10) {
				return DONE;
			}
		}		
	}
	return DONE;
}

/* sends packet called for by srej the goes to waitSREJ */
/* returns 0 on successful SREJ response and 1 on failure (timeout or other problem */
int sendSREJ(int* socketNum, struct sockaddr_in6* client, uint32_t* bufferSize, uint32_t srej, MetaData *md, int firstCall, int* srejCounter, int* transferFD)
{
	int result = 0;
	Frame srejFrame;
	int clientAddrLen = sizeof(struct sockaddr_in6);

	// pull srej packet from buffer
	srejFrame = getFrameData(srej);
	// check if valid entry
	if (srejFrame.seqNum == -1) {
		// honestly don't know what to do if no the right frame
	}
	// first time this function is called from open/closed window
	if (firstCall == 0) {
		// set counter to 0
		(*srejCounter) = 0;
	}
	// send packet requested by srej
	sendtoErr(*socketNum, srejFrame.buff, srejFrame.frameLen, 0, (struct sockaddr *) client, clientAddrLen);	
	// call waitSREJ
	return result;
}

/* checks program argument format, gets error rate and optional port number */
void checkArgs(int argc, char* argv[], struct argsInput* serverArgs)
{
	char* ptr;
	/* check number of args */
	if (argc > 3 || argc < 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	/* get error percent */
	serverArgs->errorPercent = strtod(argv[1], &ptr);
	/* get port number */
	if (argc == 3)
	{
		serverArgs->portNumber = atoi(argv[2]);
	}
	else {
		serverArgs->portNumber = 0;
	}
}


