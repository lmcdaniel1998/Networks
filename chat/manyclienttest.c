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
#define NUMCLIENTS 200

char nums[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
char Buf[MAXBUF + 100];
uint16_t inputLen = 0;
fd_set readfds;
int socketNum = 0;
uint16_t totLen = 0;
uint8_t flags = 0;
uint32_t NumClients = 0;
uint32_t recvClients = 0;

void sendToServer(int socketNum);
int establishHandle(int socketNum, char* handle);
void itoa(int num, char* buf);
void checkArgs(int argc, char * argv[]);

int main(int argc, char* argv[]) {
	int readFD = 0;
	int i, j = 0;
	char input[MAXBUF + 1];
	char buffer[4];
	char name[8];
	int sockets[NUMCLIENTS];

	checkArgs(argc, argv);

	for (i = 0; i < NUMCLIENTS; i++) {
		/* set up the TCP Client socket */
		socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
		/* establish a handle on the server */
		for (j = 0; j < 8; j++) {
			name[0] = '\0';
		}
		name[0] = 't';
		name[1] = 'e';
		name[2] = 's';
		name[3] = 't';
		name[4] = '\0';
		itoa(i, buffer);
		strcat(name, buffer);
		printf("%s\n", name);
		// establish handle on server
		establishHandle(socketNum, name);
		sockets[i] = socketNum;
	}
	int active = 0;
	char aChar = 0;
	int inputLen = 0;
	char mybuf[30];

	while (active == 0) {
		mybuf[0] = '\0';
		printf("enter anything to exit: ");
		while (inputLen < 29 && aChar !='\n') {
			aChar = getchar();
			if (aChar != '\n') {
				mybuf[inputLen] = aChar;
				inputLen++;
			}
		}
		if (inputLen > 0) {
			active = 1;
		}
	}
	for (i = 0; i < NUMCLIENTS; i++) {
		close(sockets[i]);
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

void itoa(int num, char* buf) {
	double tens = 0.0;
	double hundreds = 0.0;
	int temp1, temp2 = 0;

	buf[0] = '\0';
	buf[1] = '\0';
	buf[2] = '\0';
	buf[3] = '\0';

	if (num < 10) {
		buf[0] = nums[num];
		buf[1] = '\0';
	}
	else if (num >= 10 && num <= 99) {
		tens = (double)num * 0.1;
		temp1 = (int)tens;
		buf[0] = nums[temp1];
		buf[1] = nums[num % 10];
		buf[2] = '\0';
	}
	else if (num >= 100 && num < 999) {
		hundreds = (double)num * 0.01;
		temp2 = (int)hundreds;
		buf[0] = nums[temp2];
		temp2 = num % 100;
		tens = (double)temp2 * 0.1;
		temp1 = (int)tens;
		buf[1] = nums[temp1];
		buf[2] = nums[temp2 % 10];
		buf[3] = '\0';
	}
	else {
		return;
	}
	return;
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
