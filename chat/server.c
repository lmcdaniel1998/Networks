/******************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
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
#include "linkedlist.h"

#define DEBUG_FLAG 1

void sendToClient(int clientSD);
int checkArgs(int argc, char *argv[]);
void clearBuff(void);
int initSocketFD(void);
void clientAction(void);
int processMessage(int clientSD, uint16_t totLen, uint8_t flags);

int serverSocket = 0;
fd_set readfds;
char buf[MAXBUF + 100];
int messageLen = 0;

int main(int argc, char *argv[])
{
	int portNumber = 0;
	int maxSocket;
	int sockets;

	int clientSocket = 0;
	
	/* get portnumber for server socket */
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	serverSocket = tcpServerSetup(portNumber);	// ends with listen call

	/* add server socket to linked list as head */
	struct socketHandle *serverHandle = malloc(sizeof(struct socketHandle));
	serverHandle->handle[0] = '1';
	serverHandle->handle[6] = '\0';
	serverHandle->socketNum = serverSocket;
	serverHandle->prev = NULL;
	serverHandle->next = NULL;
	addLL(serverHandle);

	while (1) {
		/* initialize readfds */
		maxSocket = initSocketFD();
		/* wait for socket activity */
		if ((sockets = select(maxSocket + 1, &readfds, NULL, NULL, NULL)) < 0) {
			printf("Select Error\n");
		}

		/* check for incomming connections on server socket */
		clientSocket = tcpAccept(serverSocket, readfds, DEBUG_FLAG);
		if (clientSocket > 0) {
			struct socketHandle *clientHandle = malloc(sizeof(struct socketHandle));
			clientHandle->handle[0] = '\0';
			clientHandle->socketNum = clientSocket;
			clientHandle->prev = NULL;
			clientHandle->next = NULL;
			addLL(clientHandle);			
		}

		clientAction();
	}
	return 0;
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

void sendToClient(int clientSD) {
	int sent = 0;
	sent = send(clientSD, &buf[0], CHATHDRLEN, 0);
	if (sent < 0) {
		printf("message header failed to send\n");
	}
	sent += send(clientSD, &buf[CHATHDRLEN], messageLen - 3, 0);
	if (sent < 0) {
		printf("message body failed to send\n");
	}
}

int initSocketFD(void) {
	int clientSD;
	int max = serverSocket;
	FD_ZERO(&readfds);
	struct socketHandle* temp;
	/* cycle through whole linked list and add all sockets */
	temp = getFirst();
	while (temp != NULL) {
		clientSD = temp->socketNum;
		if (clientSD > 0) {
			FD_SET(clientSD, &readfds);
		}
		/* find max socket */
		if (clientSD > max) {
			max = clientSD;
		}
		temp = temp->next;
	}
	return max;
}

void clientAction(void) {
	int clientSD;
	int packetLen;
	uint16_t totLen = 0;
	uint8_t flags = 0;
	struct socketHandle* client;
	struct socketHandle* temp;
	temp = getFirst();
	/* skip over server socket */
	temp = temp->next;
	while (temp != NULL) {
		clientSD = temp->socketNum;
		if (FD_ISSET(clientSD, &readfds)) {
			/* client has terminated */
			if ((packetLen = recv(clientSD, &buf[0], CHATHDRLEN, MSG_WAITALL)) == 0) {
				/* remove client from hashtable and close socket */
				client = getLL(NULL, clientSD);
				removeLL(client);
				close(clientSD);
			}
			/* client sent a message */
			else {
				/* decode chat header */
				totLen = getTotLen(&buf[0]);
				flags = getFlags(&buf[0]);
				/* get remainder of packet */
				if (totLen > 3) {
					if ((packetLen += recv(clientSD, &buf[CHATHDRLEN], totLen - 3, MSG_WAITALL)) < 3) {
						printf("error recieving rest of packet");
					}
				}
				/* for testing */
				//printf("totlen: %d, packetLen %d, flags %d\n", totLen, packetLen, flags);
				processMessage(clientSD, totLen, flags);
			}
		}
		temp = temp->next;
	}
	return;
}

void clearBuff(void) {
	int i;
	for (i = 0; i < MAXBUF; i++) {
		buf[i] = '\0';
	}
}

void handleValid(int clientSD) {
	messageLen = CHATHDRLEN;
	clearBuff();
	buildChatHdr(messageLen, FLAG2, &buf[0]);
	sendToClient(clientSD);
	messageLen = 0;
}

void handleInvalid(int clientSD) {
	messageLen = CHATHDRLEN;
	clearBuff();
	buildChatHdr(messageLen, FLAG3, &buf[0]);
	sendToClient(clientSD);
	messageLen = 0;
	close(clientSD);
}

void validateHandle(int clientSD, uint16_t totLen) {
	int i = 0;
	char handle[101];
	getHandle(CHATHDRLEN, &handle[0], buf);
	uint8_t handleLen = strlen(handle);

	struct socketHandle* client;
	/* search linked list for handle, if handle exists send handle Invalid */
	if (getLL(handle, -1) == NULL) {
		/* find struct for clientSD, if non existant ingnore */
		if ((client = getLL(NULL, clientSD)) != NULL) {
			for (i = 0; i < handleLen; i++) {
				client->handle[i] = handle[i];
			}
			memcpy(&(client->handle[0]), &handle[0], handleLen);
			handleValid(clientSD);
		}
	}
	/* handle has been taken, remove client form linked list and report error */
	else {
		if ((client = getLL(NULL, clientSD)) != NULL) {
			removeLL(client);
			handleInvalid(clientSD);
		}
	}
}

void clientMessageError(int clientSD, uint16_t totLen, char* errorHandle) {
	char holdBuf[MAXBUF];
	uint16_t holdMessageLen = messageLen;
	uint8_t handleLen = strlen(errorHandle);
	/* save client packet for other other destinations */
	memcpy(&holdBuf[0], &buf[0], totLen);
	messageLen = 0;
	clearBuff();
	/* build client message error packet */
	buildChatHdr(0, FLAG7, &buf[0]);
	messageLen = CHATHDRLEN;
	messageLen = buildHandleHdr(messageLen, errorHandle, handleLen, buf);
	setTotLen(messageLen, &buf[0]);
	sendToClient(clientSD);
	/* restore old packet */
	memcpy(&buf[0], &holdBuf[0], totLen);
	messageLen = holdMessageLen;
}

void broadCast(int clientSD, uint16_t totLen) {
	uint16_t index;
	char sendingClient[101];
	index = CHATHDRLEN;
	/* get start of linked list */
	struct socketHandle* temp = getFirst();
	/* skip over server fd */
	temp = temp->next;

	/* get sending client handle */
	getHandle(index, &sendingClient[0], buf);
	/* go through linked list and send to clients */
	while (temp != NULL) {
		/* don't broadcast to sending client */
		if (strcmp(&sendingClient[0], &(temp->handle[0])) != 0) {
			messageLen = totLen;
			sendToClient(temp->socketNum);
		}
		temp = temp->next;
	}
}

void forwardMessage(int clientSD, uint16_t totLen) {
	uint16_t index;
	uint8_t sendingClientLen = getHandleLen(CHATHDRLEN, buf);
	char charClientNum;
	uint8_t clientNum = 0;
	uint8_t temp;
	char destClients[MAXDESTINATIONS][101];
	char destClt[101];
	uint8_t destClientLen = 0;
	struct socketHandle* client;
	int i;
	/* get number of destination clients */
	index = sendingClientLen + 4;
	memcpy(&charClientNum, &buf[index], 1);
	clientNum = charClientNum;
	index++;
	/* get all dest clients */
	for (temp = 0; temp < MAXDESTINATIONS; temp++) {
		destClients[temp][0] = '\0';
	}
	temp = clientNum;
	while (temp > 0) {
		destClientLen = getHandleLen(index, buf);
		getHandle(index, &destClt[0], buf);
		for (i = 0; i < destClientLen; i++) {
			destClients[temp - 1][i] = destClt[i];
		}
		destClients[temp - 1][destClientLen] = '\0';
		index += (1 + destClientLen);
		temp--;
	}
	/* forward packet to all clients */
	for (temp = 0; temp < MAXDESTINATIONS; temp++) {
		if (destClients[temp][0] != '\0') {
			/* socket for handle found */
			client = getLL(&(destClients[temp][0]), -1);
			if (client != NULL) {
				/* forward packet */
				messageLen = totLen;
				sendToClient(client->socketNum);
			}
			/* no socket for handle found */
			else {
				/* send error packet flags = 7 to sender */
				clientMessageError(clientSD, totLen, &destClients[temp][0]);
			}
		}
	}
}

void removeC(int clientSD) {
	struct socketHandle* client;
	/* build remove confirmation header flags = 9 */
	buildChatHdr(CHATHDRLEN, FLAG9, &buf[0]);
	messageLen = CHATHDRLEN;
	sendToClient(clientSD);
	/* remove client data */
	client = getLL(NULL, clientSD);
	/* dont remove sd yet just remove handle from linked list */
	/* client action will delete item from linked list when 0 is received */
	client->handle[0] = '\0';
}

void sendClients(int clientSD) {
	/* subtract 1 for server */
	uint32_t numClientsH = ((uint32_t)getNumItems()) - 1;
	uint32_t numClientsN = htonl(numClientsH);
	uint32_t byteSelect = 0xff000000;
	uint8_t byte;
	int i, j;
	struct socketHandle* temp;
	char* handle;
	uint8_t handleLen = 0;
	uint16_t endIdx = 0;
	/* build packet flags = 11 */
	buildChatHdr((CHATHDRLEN + 4), FLAG11, &buf[0]);
	/* add bytes of numClients to packet */
	j = 3;
	for (i = 0; i < 4; i++) {
		byte = ((uint8_t)((numClientsN & (byteSelect >> (8 * i))) >> (8 * j)));
		memcpy(&buf[CHATHDRLEN + i], &byte, 1);
		j--;
	}
	/* send packet flags = 11 to client */
	messageLen = 7;
	sendToClient(clientSD);
	/* get and send all clients flags = 12 */
	temp = getFirst();
	/* skip over server in linked list */
	temp = temp->next;
	/* build chat header with no length */
	buildChatHdr(0, FLAG12, &buf[0]);
	while (temp != NULL) {
		handle = &(temp->handle[0]);
		handleLen = strlen(handle);
		endIdx = buildHandleHdr(CHATHDRLEN, handle, handleLen, &buf[0]);
		/* set length field in packet */
		setTotLen(endIdx, &buf[0]);
		messageLen = endIdx;
		sendToClient(clientSD);
		temp = temp->next;
	}
	/* build and send end of client flags = 13 */
	buildChatHdr(CHATHDRLEN, FLAG13, &buf[0]);
	messageLen = CHATHDRLEN;
	sendToClient(clientSD);
}


int processMessage(int clientSD, uint16_t totLen, uint8_t flags) {
	/* flags = 1 client is trying to secure a handle */
	if (flags == FLAG1) {
		validateHandle(clientSD, totLen);
	}
	/* flags = 4 broadcast message to all clients */
	else if (flags == FLAG4) {
		broadCast(clientSD, totLen);
	}
	/* flags = 5 client is sending a message */
	else if (flags == FLAG5) {
		forwardMessage(clientSD, totLen);
	}
	/* flags = 8 client wants to exit */
	else if (flags == FLAG8) {
		removeC(clientSD);
	}
	/* flags = 10 client requesting client list */
	else if (flags == FLAG10) {
		sendClients(clientSD);
	}
	return 0;
}
