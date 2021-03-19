/******************************************************************************
* myClient.c
*
*****************************************************************************/

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

#include "networks.h"
#include "sharedFuncs.h"

#define DEBUG_FLAG 1
#define MAXHANDLELEN 100

char Buf[MAXBUF + 100];
uint16_t inputLen = 0;
fd_set readfds;
int socketNum = 0;
uint16_t totLen = 0;
uint8_t flags = 0;
uint32_t NumClients = 0;
uint32_t recvClients = 0;

int initClientFD(void);
int initProcessFD(void);
void sendToServer(int socketNum);
void recvFromServer(int socketNum);
void checkArgs(int argc, char * argv[]);
void clearBuff(void);
void clearInput(char input[]);
int establishHandle(int socketNum, char* handle);
int verifyHandle(int socketNum, char* handle);
void processInput(int socketNum, char* handle, char input[]);
void processMessage(void);

int main(int argc, char * argv[])
{	
	int readFD = 0;
	int sockets = 0;

	char input[MAXBUF + 1];

	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	
	/* establish a handle on the server */
	establishHandle(socketNum, argv[1]);
	/* verify handle availablility */
	readFD = initClientFD();
	if ((sockets = select(readFD + 1, &readfds, NULL, NULL, NULL)) < 0) {
		printf("Select Error\n");
	}
	if (FD_ISSET(readFD, &readfds)) {
		recvFromServer(socketNum);
		if (verifyHandle(readFD, argv[1]) == -1) {
			exit(0);
		}
	}
	/* send and recieve messages from server */
	while(1) {
		/* prompt client for command */
		printf("$: ");
		fflush(stdout);
		/* add stdin and socket to readfds */
		readFD = initProcessFD();
		if ((sockets = select(readFD + 1, &readfds, NULL, NULL, NULL)) < 0) {
			printf("Select Error\n");
		}
		/* incoming data from server */
		if (FD_ISSET(readFD, &readfds)) {
			recvFromServer(socketNum);
			processMessage();
		}
		/* user command input */
		if (FD_ISSET(STDIN_FILENO, &readfds)) {
			inputLen = read(STDIN_FILENO, &input[0], MAXBUF);
			if (inputLen == -1) {
				perror("read");
				exit(-1);
			}
			input[inputLen] = '\0';
			inputLen++;
			processInput(socketNum, argv[1], &input[0]);
		}
		clearInput(input);
		inputLen = 0;
	}
	return 0;
}

int initClientFD(void) {
	int serverSD;
	FD_ZERO(&readfds);
	FD_SET(socketNum, &readfds);
	serverSD = socketNum;
	return serverSD;
}

int initProcessFD(void) {
	int inputSD;
	FD_ZERO(&readfds);
	FD_SET(socketNum, &readfds);
	FD_SET(STDIN_FILENO, &readfds);
	if (STDIN_FILENO > socketNum) {
		inputSD = STDIN_FILENO;
	}
	else {
		inputSD = socketNum;
	}
	return inputSD;
}

void sendToServer(int socketNum)
{
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
		
	sent =  send(socketNum, &Buf[0], CHATHDRLEN, 0);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}
	sent += send(socketNum, &Buf[CHATHDRLEN], totLen - 3, 0);
	if (sent < 3) {
		perror("send call");
		exit(-1);
	}
}

void recvFromServer(int socketNum)
{
	int recieved = 0;

	recieved = recv(socketNum, &Buf[0], CHATHDRLEN, MSG_WAITALL);
	/* server has terminated */
	if (recieved == 0) {
		printf("Server Terminated\n");
		exit(0);
	}
	/* decode chat header */
	totLen = getTotLen(&Buf[0]);
	flags = getFlags(&Buf[0]);
	/* get remainder of message */
	if (totLen > 3) {
		recieved += recv(socketNum, &Buf[CHATHDRLEN], totLen - 3, MSG_WAITALL);
		if (recieved < 0) {
			perror("recv call");
			exit(-1);
		}
	}
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}
	if (argv[1][0] >= '0' && argv[1][0] <= '9') {
		printf("Invalid handle, handle starts with a number\n");
		exit(1);
	}
}

void clearBuff(void) {
	int i;
	for (i = 0; i < MAXBUF; i++) {
		Buf[i] = '\0';
	}
}

void clearInput(char input[]) {
	int i;
	for (i = 0; i < (MAXBUF + 1); i++) {
		input[i] = '\0';
	}
}

int establishHandle(int socketNum, char* handle) {
	uint8_t handleLen = strlen(handle);
	totLen = CHATHDRLEN + 1 + handleLen;
	/* construct chat header */
	buildChatHdr(totLen, FLAG1, &Buf[0]);
	/* add requested handle to header */
	buildHandleHdr(CHATHDRLEN, handle, handleLen, &Buf[0]);
	/* send handle request to server */
	sendToServer(socketNum);
	return totLen;
}

int verifyHandle(int socketNum, char* handle) {
	/* handle requested is available */
	if (flags == FLAG2) {
		printf("your handle is: %s\n", handle);
		return 0;
	}
	if (flags == FLAG3) {
		printf("Handle already in use: %s\n", handle);
		return -1;
	}
	return 0;
}

void sendClient(int socketNum, char* handle, char* input) {
	uint16_t index = 0;
	uint16_t indexHold = 0;
	uint16_t inputIndex = 0;
	uint16_t messageStart = 0;
	char charClients;
	uint8_t numClients = 0;
	char* destHandle;
	uint8_t handleLen = 0;
	uint16_t mesgLen = 0;
	uint16_t mesgSegLen = 0;
	char inputHold[MAXBUF + 1];
	char newLine = '\n';
	char nullTerm = '\0';
	clearBuff();
	/* build chat header but no totLen yet */
	buildChatHdr(0, FLAG5, Buf);
	index = CHATHDRLEN;
	/* add in sending client */
	handleLen = strlen(handle);
	index = buildHandleHdr(index, handle, handleLen, Buf);
	/* get number of dest handles and place in header */
	memcpy(&charClients, &input[inputIndex], 1);
	numClients = charClients - '0';
	if (numClients > MAXDESTINATIONS) {
		numClients = MAXDESTINATIONS;
		printf("too many clients specified, only sending to first 9\n");
	}
	inputIndex += 2;
	memcpy(&Buf[index], &numClients, 1);
	index++;
	/* get clients from input */
	memcpy(&inputHold[0], &input[0], strlen(&input[0]));
	destHandle = strtok(&inputHold[inputIndex], " ");
	handleLen = strlen(destHandle);
	while (numClients > 0 && destHandle != NULL) {
		/* handle is less than 100 characters */
		if (handleLen <= MAXHANDLELEN) {
			/* add handle and length to packet */
			index = buildHandleHdr(index, destHandle, handleLen, Buf);
			inputIndex += (handleLen + 1);
		}
		else {
			printf("Invalid handle, handle linger than 100 characters: %s\n", destHandle);
			inputIndex += (handleLen + 1);
		}
		destHandle = strtok(NULL, " ");
		handleLen = strlen(destHandle);
		numClients--;
	}
	/* set totLen and get start of message */
	messageStart = inputIndex;
	while (input[inputIndex] != '\0') {
		inputIndex++;
		mesgLen++;
	}
	/* prevents sending empty messages */
	if (mesgLen == 0) {
		return;
	}
	inputIndex = messageStart;
	/* place message into packet */
	indexHold = index;
	if (mesgLen > MAXMESSAGE) {
		indexHold = index;
		while (mesgLen > MAXMESSAGE) {
			/* add message segment to packet */
			mesgSegLen = MAXMESSAGE;
			index = indexHold;
			memcpy(&Buf[index], &input[inputIndex], mesgSegLen);
			memcpy(&Buf[(index + mesgSegLen + 1)], &newLine, 1);
			memcpy(&Buf[(index + mesgSegLen + 2)], &nullTerm, 1);
			/* set totLen of packet */
			index += (mesgSegLen + 2);
			totLen = index;
			setTotLen(index, &Buf[0]);
			sendToServer(socketNum);
			inputIndex += MAXMESSAGE;
			mesgLen -= MAXMESSAGE;
		}
	}
	memcpy(&Buf[indexHold], &input[inputIndex], mesgLen);
	/* add null terminator */
	Buf[indexHold + mesgLen - 1] = '\0';
	/* set totLen */
	indexHold += mesgLen;
	totLen = indexHold;
	setTotLen(indexHold, Buf);
	sendToServer(socketNum);
}

void requestClients(int socketNum) {
	/* build header flags = 10 */
	buildChatHdr(CHATHDRLEN, FLAG10, &Buf[0]);
	totLen = CHATHDRLEN;
	sendToServer(socketNum);
}

void requestExit(int socketNum) {
	/* build header flags = 8 */
	buildChatHdr(CHATHDRLEN, FLAG8, &Buf[0]);
	totLen = CHATHDRLEN;
	sendToServer(socketNum);
}

void sendBroadcast(int socketNum, char* handle, char input[]) {
	uint16_t index = 0;
	uint16_t indexHold = 0;
	uint16_t inputIndex = 0;
	uint8_t handleLen = 0;
	uint16_t mesgLen = 0;
	uint16_t mesgSegLen = 0;
	char newLine = '\n';
	char nullTerm = '\0';
	clearBuff();
	/* build chat header but no totLen yet */
	buildChatHdr(0, FLAG4, Buf);
	index = CHATHDRLEN;
	/* add in sending client */
	handleLen = strlen(handle);
	index = buildHandleHdr(index, handle, handleLen, Buf);
	/* prevents sending empty message */
	if ((int)strlen(input) == 0) {
		return;
	}
	/* set totLen and get start of message */
	while (input[mesgLen] != '\0') {
		mesgLen++;
	}
	/* place message into packet */
	indexHold = index;
	if (mesgLen > MAXMESSAGE) {
		indexHold = index;
		while (mesgLen > MAXMESSAGE) {
			/* add message segment to packet */
			mesgSegLen = MAXMESSAGE;
			index = indexHold;
			memcpy(&Buf[index], &input[inputIndex], mesgSegLen);
			memcpy(&Buf[(index + mesgSegLen + 1)], &newLine, 1);
			memcpy(&Buf[(index + mesgSegLen + 2)], &nullTerm, 1);
			/* set totLen of packet */
			index += (mesgSegLen + 1);
			totLen = index;
			setTotLen(index, &Buf[0]);
			sendToServer(socketNum);
			inputIndex += MAXMESSAGE;
			mesgLen -= MAXMESSAGE;
		}
	}
	memcpy(&Buf[indexHold], &input[inputIndex], mesgLen);
	/* set totLen */
	indexHold += mesgLen;
	totLen = indexHold;
	setTotLen(indexHold, Buf);
	sendToServer(socketNum);
}

void processInput(int socketNum, char* handle, char input[]) {
	uint16_t index = 0;
	char command[3];
	char* MCommand = "%M";
	char* mCommand = "%m";
	char BCommand[3] = {'%', 'B', '\0'};
	char bCommand[3] = {'%', 'b', '\0'};
	char* LCommand = "%L";
	char* lCommand = "%l";
	char ECommand[3] = {'%', 'E', '\0'};
	char eCommand[3] = {'%', 'e', '\0'};
	
	/* get command */
	while (input[index] != ' ' && input[index] != '\n') {
		/* not a proper command */
		if (index > 3) {
			printf("Invalid command format\n");
			return;
		}
		command[index] = input[index];
		index++;
	}
	command[index] = '\0';
	/* determine command */
	if (strcmp(command, MCommand) == 0 || strcmp(command, mCommand) == 0) {
		//printf("input passed to sendClient: %s\n", &input[index + 1]);
		sendClient(socketNum, handle, &input[index + 1]);
	}
	else if (strcmp(command, LCommand) == 0 || strcmp(command, lCommand) == 0) {
		requestClients(socketNum);
	}
	else if (strcmp(command, &ECommand[0]) == 0 || strcmp(command, &eCommand[0]) == 0) {
		requestExit(socketNum);
	}
	else if (strcmp(command, &BCommand[0]) == 0 || strcmp(command, &bCommand[0]) == 0) {
		sendBroadcast(socketNum, handle, &input[index + 1]);
	}

	else {
		printf("Invalid command\n");
	}
}

void broadcastRecv(void) {
	char message[MAXMESSAGE + 1];
	uint16_t index = CHATHDRLEN;
	char sendingClient[101];
	getHandle(CHATHDRLEN, &sendingClient[0], Buf);
	uint8_t handleLen = getHandleLen(CHATHDRLEN, Buf);
	int i; 
	index += (handleLen + 1);
	/* get message */
	memcpy(&message[0], &Buf[index], (totLen - index));
	message[(totLen - index) + 1] = '\0';
	printf("\n%s : ", &sendingClient[0]);
	for (i = 0; i < (totLen - index); i++) {
		 printf("%c", message[i]);
	}
}

void clientMessage(void) {
	char message[MAXMESSAGE + 1];
	uint16_t index = CHATHDRLEN;
	char sendingClient[101];
	getHandle(CHATHDRLEN, &sendingClient[0], Buf);
	uint8_t handleLen = getHandleLen(CHATHDRLEN, Buf);
	uint8_t recvClients = 0;
	int i; 
	/* get number of clients message was sent to */
	index += (1 + handleLen);
	recvClients = Buf[index];
	index++;
	/* go over other recipients until message */
	while (recvClients > 0) {
		handleLen = getHandleLen(index, Buf);
		index += (1 + handleLen);
		recvClients--;
	}
	/* get message */
	memcpy(&message[0], &Buf[index], (totLen - index));
	message[(totLen - index) + 1] = '\0';
	/* print sender and message */
	printf("\n%s : ", &sendingClient[0]);
	for (i = 0; i < (totLen - index); i++) {
		 printf("%c", message[i]);
	}
	printf("\n");
}

void messageError(void) {
	char errorClient[101];
	getHandle(CHATHDRLEN, &errorClient[0], &Buf[0]);
	printf("Client with handle %s does not exist.\n", &errorClient[0]);
}

void getNumClients(void) {
	uint32_t numClientsN = 0;
	uint8_t byteChar;
	int i, j;
	j = 3;
	for (i = 0; i < 4; i++) {
		memcpy(&byteChar, &Buf[CHATHDRLEN + i], 1);
		numClientsN = numClientsN | (((uint32_t)byteChar) << (8 * j));
		j--;
	}
	NumClients = ntohl(numClientsN);
	recvClients = 0;
	printf("\b\b\b");
	fflush(stdout);
}

void getClient(void) {
	char handle[101];
	/* get client handle and print out */
	getHandle(CHATHDRLEN, &handle[0], &Buf[0]);
	recvClients++;
	printf("\b \b\b \b\b \b%s\n", &handle[0]);
}

void endClients(void) {
	if (NumClients != recvClients) {
		printf("\b\b\bincorrect number of handles recieved\n");
	}
	else {
		printf("\b\b\b");
		fflush(stdout);
	}
}

void processMessage(void) {
	/* broadcast message from another client */
	if (flags == FLAG4) {
		broadcastRecv();
	}
	/* message from another client */
	else if (flags == FLAG5) {
		clientMessage();
	}
	/* error in sending message to client */
	else if (flags == FLAG7) {
		messageError();
	}
	/* client can now close */
	else if (flags == FLAG9) {
		exit(0);
	}
	/* client handles incoming */
	else if (flags == FLAG11) {
		getNumClients();
	}
	/* client handle */
	else if (flags == FLAG12) {
		getClient();
	}
	/* end of client handles */
	else if (flags == FLAG13) {
		endClients();
	}
	else {
		printf("packet flag not recognized\n");
	}
}
