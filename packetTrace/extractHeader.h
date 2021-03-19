/*---------------------------------------------------*/
/*
 * author: Luke McDaniel
 * institution: California Polytechnic State University
 * course: CPE 464-01 (Networks)
 * assignment: Program 1 (Packet Trace)
*/
/*-------- extractHeader.h of trace program ---------*/

/* ether library in header for data structure */
#include <netinet/ether.h>
/* internet protocol library for data structure */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* header for integer size types */
#include <stdint.h>

/* data structure to store ethernet header */
typedef struct __attribute__((__packed__))EtherHeader {
	const struct ether_addr destAddr;
	const struct ether_addr sourceAddr;
	uint16_t protocol;
}EtherHeader;

/* data structure to store ARP header */
typedef struct __attribute__((__packed__))ARPHeader {
	uint16_t hwType;
	uint16_t protocolType;
	uint8_t hwSize;
	uint8_t protocolSize;
	uint16_t opCode;
	const struct ether_addr sourceMAC;
	const struct in_addr sourceIP;
	const struct ether_addr targetMAC;
	const struct in_addr targetIP;
}ARPHeader;

/* data structure to store IP header */
typedef struct __attribute__((__packed__))IPHeader {
	uint8_t verAndHdr;
	uint8_t tos;
	uint16_t totLen;
	uint16_t id;
	uint16_t flags;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	const struct in_addr senderIP;
	const struct in_addr destIP;
}IPHeader;

/* data structure to store TCP header */
typedef struct __attribute__((__packed__))TCPHeader {
	uint16_t sourcePort;
	uint16_t destPort;
	uint32_t seqNumber;
	uint32_t ackNumber;
	uint16_t hdrAndFlags;
	uint16_t windowSize;
	uint8_t checksum1;
	uint8_t checksum2;
}TCPHeader;

/* data structure to store UDP header */
typedef struct __attribute__((__packed__))UDPHeader {
	uint16_t sourcePort;
	uint16_t destPort;
	uint16_t length;
	uint16_t checksum;
}UDPHeader;

/* data structure to store ICMP header */
typedef struct __attribute__((__packed__))ICMPHeader {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
}ICMPHeader;

void extractHeaders(u_char *data);
