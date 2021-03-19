/*********************************************************/
/* Shared Functions in Program 2 Chat
 * Author: Luke McDaniel
 * Institution: California Polytechnic State University
 * Course: CPE 464-01 (Networks)
*/
/*********************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define FIRST 0xff00
#define SECOND 0x00ff
#define MAXHANDLE 100

/* takes length in host order */
/* sets total legth field in header (network order) */
void setTotLen(uint16_t length, char* packet) {
	uint16_t lengthNetworkOrder = htons(length);
	memcpy(packet, &lengthNetworkOrder, 2);
}

/* takes length and flags is host order */
/* sets total length and flags in header */
void buildChatHdr(uint16_t length, uint8_t flags,
char* packet) {
	setTotLen(length, packet);
	packet[2] = flags;
}

/* extracts total length from header */
uint16_t getTotLen(char* packet) {
	uint16_t length;
	memcpy(&length, packet, 2);
	return ntohs(length);
}

/* extracts flags from header */
uint8_t getFlags(char* packet) {
	uint8_t flags = packet[2];
	return flags;
}

/* places handle length and handle into header
 * starting at startIdx */
/* return index after handler */
uint16_t buildHandleHdr(uint16_t startIdx, char* handle,
uint8_t handleLen, char* packet) {
	uint16_t endIdx = startIdx;
	uint8_t length = handleLen;
	memcpy(&packet[endIdx], &length, 1);
	endIdx += 1;
	memcpy(&packet[endIdx], handle, length);
	endIdx += length;
	return endIdx;
}

uint8_t getHandleLen(uint16_t startIdx, char* packet) {
	uint8_t handleLen = 0;
	memcpy(&handleLen, &packet[startIdx], 1);
	return handleLen;
}

/* gets handle at at startIdx */
void getHandle(uint16_t startIdx, char* handle, char* packet) {
	uint16_t idx = startIdx;
	uint8_t handleLen;
	char * nullTerm = "\0";
	memcpy(&handleLen, &packet[idx], 1);
	idx += 1;
	memcpy(&handle[0], &packet[idx], handleLen);
	memcpy(&handle[handleLen], nullTerm, 1);
}
