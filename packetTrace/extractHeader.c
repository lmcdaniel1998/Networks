/*---------------------------------------------------*/
/*
 * author: Luke McDaniel
 * institution: California Polytechnic State University
 * course: CPE 464-01 (Networks)
 * assignment: Program 1 (Packet Trace)
*/
/*-------- extractHeader.c of trace program ---------*/

#include "extractHeader.h"
/* use pcap library for packet sniffing
 * (must be installed on machine) */
#include <pcap/pcap.h>
/* file I/O and string processing libraries */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* internet checksum for IP */
#include "checksum.h"


/* Type values for ethernet header */
#define ARP 0x0806
#define IP 0x0800

/* arp opcode types */
#define ARPRequest 0x0001
#define ARPReply 0x0002

/* tcp header and flags */
#define TCPHDRlen 0xf000
#define TCPACKflag 0x0010
#define TCPRSTflag 0x0004
#define TCPSYNflag 0x0002
#define TCPFINflag 0x0001

/* ip protocol types */
#define TCP 1
#define UDP 2
#define ICMP 3

/* icmp types */
#define REPLY 0x00
#define REQUEST 0x08

/* HTTP port */
#define HTTP 80

/* Starts of different headers */
#define startNext 14
#define sudoHdrSize 12

/* stores length of frame */
uint32_t FrameLen = 0;

/* functions for converting header data from
 * network to machine and vice-versa */

/* ethernet header conversion */
void ntohEther(struct EtherHeader *eth) {
	eth->protocol = ntohs(eth->protocol);
}

void htonEther(struct EtherHeader *eth) {
	eth->protocol = htons(eth->protocol);
}

/* arp header conversion */
void ntohARP(struct ARPHeader *arp) {
	arp->hwType = ntohs(arp->hwType);
	arp->protocolType = ntohs(arp->protocolType);
	arp->opCode = ntohs(arp->opCode);
}

void htonARP(struct ARPHeader *arp) {
	arp->hwType = htons(arp->hwType);
	arp->protocolType = htons(arp->protocolType);
	arp->opCode = htons(arp->opCode);
}

/* ip header conversion */
void ntohIP(struct IPHeader *ip) {
	ip->totLen = ntohs(ip->totLen);
	ip->id = ntohs(ip->id);
	ip->flags = ntohs(ip->flags);
}

void htonIP(struct IPHeader *ip) {
	ip->totLen = htons(ip->totLen);
	ip->id = htons(ip->id);
	ip->flags = htons(ip->flags);
}

/* tcp header conversion */
void ntohTCP(struct TCPHeader *tcp) {
	tcp->sourcePort = ntohs(tcp->sourcePort);
	tcp->destPort = ntohs(tcp->destPort);
	tcp->seqNumber = ntohl(tcp->seqNumber);
	tcp->ackNumber = ntohl(tcp->ackNumber);
	tcp->hdrAndFlags = ntohs(tcp->hdrAndFlags);
	tcp->windowSize = ntohs(tcp->windowSize);
}

void htonTCP(struct TCPHeader *tcp) {
	tcp->sourcePort = htons(tcp->sourcePort);
	tcp->destPort = htons(tcp->destPort);
	tcp->seqNumber = htonl(tcp->seqNumber);
	tcp->ackNumber = htonl(tcp->ackNumber);
	tcp->hdrAndFlags = htons(tcp->hdrAndFlags);
	tcp->windowSize = htons(tcp->windowSize);
}

/* udp header conversion */
void ntohUDP(struct UDPHeader *udp) {
	udp->sourcePort = ntohs(udp->sourcePort);
	udp->destPort = ntohs(udp->destPort);
	udp->length = ntohs(udp->length);
}

void htonUDP(struct UDPHeader *udp) {
	udp->sourcePort = htons(udp->sourcePort);
	udp->destPort = htons(udp->destPort);
	udp->length = htons(udp->length);
}

/* icmp header conversion */
void ntohICMP(struct ICMPHeader *icmp) {
	icmp->type = ntohs(icmp->type);
	icmp->code = ntohs(icmp->code);
}

void htonICMP(struct ICMPHeader *icmp) {
	icmp->type = htons(icmp->type);
	icmp->code = htons(icmp->code);
}

/* helper functions for extractIP */
/* gets IP header length in bytes */
int getIpHdrLen(uint8_t verAndHdr) {
	int length = 0;
	uint8_t crop = verAndHdr & (0x0f);
	length = 4 * crop;
	return length;
}

/* determine IP header protocol */
int getProtocol(uint8_t protocol) {
	if (protocol == 0x06) {
		printf("		Protocol: TCP\n");
		return TCP;
	}
	else if (protocol == 0x11) {
		printf("		Protocol: UDP\n");
		return UDP;
	}
	else if (protocol == 0x01) {
		printf("		Protocol: ICMP\n");
		return ICMP;
	}
	else {
		printf("		Protocol: Unknown\n");
		return 0;
	}
}

/* helper functions for extractTCP */
/* gets tcp header length */
int getTcpHdrLen(uint16_t hdrAndFlags) {
	int len = hdrAndFlags & TCPHDRlen;
	len = len >> 12;
	return len * 4;
}
/* get ack valid flag 1=valid 0=not valid */
int getAckFlag(uint16_t hdrAndFlags) {
	int ack = hdrAndFlags & TCPACKflag;
	ack = ack >> 4;
	return ack;
}
/* get rst valid flag 1=valid 0=not valid */
int getRstFlag(uint16_t hdrAndFlags) {
	int rst = hdrAndFlags & TCPRSTflag;
	rst = rst >> 2;
	return rst;
}
/* get syn valid flag 1=valid 0=not valid */
int getSynFlag(uint16_t hdrAndFlags) {
	int syn = hdrAndFlags & TCPSYNflag;
	syn = syn >> 1;
	return syn;
}
/* get fin valid flag 1=valid 0=not valid */
int getFinFlag(uint16_t hdrAndFlags) {
	int fin = hdrAndFlags & TCPFINflag;
	return fin;
}
/* print out TCP flag values */
void printTcpFlags(uint16_t hdrAndFlags, uint32_t ackNumber) {
	int ackF;
	int synF;
	int rstF;
	int finF;
	/* ack flag */
	ackF = getAckFlag(hdrAndFlags);
	printf("		ACK Number: ");
	if (ackF == 1) {
		printf("%u\n", ackNumber);
		printf("		ACK Flag: Yes\n");
	}
	else {
		printf("<not valid>\n");
		printf("		ACK Flag: No\n");
	}
	/* syn flag */
	synF = getSynFlag(hdrAndFlags);
	printf("		SYN Flag: ");
	if (synF == 1) {
		printf("Yes\n");
	}
	else {
		printf("No\n");
	}
	/* rst flag */
	rstF = getRstFlag(hdrAndFlags);
	printf("		RST Flag: ");
	if (rstF == 1) {
		printf("Yes\n");
	}
	else {
		printf("No\n");
	}
	/* fin flag */
	finF = getFinFlag(hdrAndFlags);
	printf("		FIN Flag: ");
	if (finF == 1) {
		printf("Yes\n");
	}
	else {
		printf("No\n");
	}
	return;
}

/* gets ip in network form, tcp in host form */
/* returns both in network form */
void tcpChecksum(u_char *data,
struct IPHeader *ip, struct TCPHeader *tcp) {
	uint16_t ipTotLen;
	uint16_t ipHeaderLen;
	uint16_t tcpAndDataLen;
	uint16_t reserved = 0x0000;
	uint16_t tmp = 0x0000;
	uint8_t hdrLen1 = 0x00;
	uint8_t hdrLen2 = 0x00;
	int check;
	u_char *sudoHeader;
	u_char *totChecksum;
	/* get lengths of headers and data */
	ntohIP(ip);
	ntohTCP(tcp);
	ipTotLen = ip->totLen;
	ipHeaderLen = getIpHdrLen(ip->verAndHdr);
	tcpAndDataLen = ipTotLen - ipHeaderLen;
	/* create sudo header */
	htonIP(ip);
	htonTCP(tcp);
	sudoHeader = malloc(sudoHdrSize * sizeof(u_char));
	memcpy(sudoHeader, ((u_char*)(&(ip->senderIP))), 4);
	memcpy(sudoHeader + 4, ((u_char*)(&(ip->destIP))), 4);
	memcpy(sudoHeader + 8, &reserved, 1);
	memcpy(sudoHeader + 9, ((u_char*)(&(ip->protocol))), 1);
	/* format tcp and data len */
	if ((tcpAndDataLen & 0xff00) > 0) {
		tmp = tcpAndDataLen & 0xff00;
		tmp = tmp >> 8;
		hdrLen1 = (uint8_t)tmp;
		memcpy(sudoHeader + 10, &hdrLen1, 1);
		tmp = tcpAndDataLen & 0x00ff;
		hdrLen2 = (uint8_t)tmp;
		memcpy(sudoHeader + 11, &hdrLen2, 1);
	}
	else {
		memcpy(sudoHeader + 10, &reserved, 1);
		memcpy(sudoHeader + 11, &tcpAndDataLen, 1);
	}
	/* create checksum header */
	totChecksum = malloc((sudoHdrSize + tcpAndDataLen)
	* sizeof(u_char));
	memcpy(totChecksum, sudoHeader, sudoHdrSize);
	memcpy(totChecksum + sudoHdrSize, data, tcpAndDataLen);
	/* perform checksum calculation */
	check = in_cksum(((unsigned short *)(totChecksum)),
	(sudoHdrSize + tcpAndDataLen));
	if (check == 0) {
		printf("		Checksum: Correct ");
	}
	else {
		printf("		Checksum: Incorrect ");
	}
	if (tcp->checksum1 == 0) {
		printf("(0x%.2x)", tcp->checksum2);
	}
	else {
		printf("(0x%x%.2x)", tcp->checksum1, tcp->checksum2);
	}
	return;
}

void extractTCP(u_char *data, struct IPHeader *ip) {
	struct TCPHeader *tcp;	
	/* get TCP header info */
	printf("	TCP Header\n");
	tcp = (TCPHeader *)data;
	ntohTCP(tcp);
	/* get TCP header info */
	if (tcp->sourcePort == HTTP) {
		printf("		Source Port:  HTTP\n");
	}
	else {
		printf("		Source Port: : %d\n", tcp->sourcePort);
	}
	if (tcp->destPort == HTTP) {
		printf("		Dest Port:  HTTP\n");
	}
	else {
		printf("		Dest Port: : %d\n", tcp->destPort);
	}
	printf("		Sequence Number: %u\n", tcp->seqNumber);
	/* extract flags */
	printTcpFlags(tcp->hdrAndFlags, tcp->ackNumber);
	/* window size */
	printf("		Window Size: %u\n", tcp->windowSize);
	/* calculate checksum */
	htonTCP(tcp);
	tcpChecksum(data, ip, tcp);	
}

void extractUDP(u_char *data) {
	struct UDPHeader *udp;
	/* get UDP header info */
	printf("	UDP Header\n");
	udp = (UDPHeader *)data;
	ntohUDP(udp);
	/* print source and dest ports */
	printf("		Source Port: : %d\n", udp->sourcePort);
	printf("		Dest Port: : %d", udp->destPort);
}

void extractICMP(u_char *data) {
	struct ICMPHeader *icmp;
	/* get ICMP header info */
	printf("	ICMP Header\n");
	icmp = (ICMPHeader *)data;
	/* get type */
	if (icmp->type == REQUEST) {
		printf("		Type: Request");
	}
	else if (icmp->type == REPLY) {
		printf("		Type: Reply");
	}
	else {
		printf("		Type: %d", icmp->type);
	}
}

void extractIP(u_char *data) {
	struct IPHeader *ip;
	int headerLen;
	int protocol = 0;
	unsigned short *addr;
	/* get IP header info */
	printf("\n	IP Header\n");
	ip = (IPHeader *)data;
	ntohIP(ip);
	headerLen = getIpHdrLen(ip->verAndHdr);
	printf("		Header Len: %d (bytes)\n", headerLen);
	printf("		TOS: 0x%x\n", ip->tos);
	printf("		TTL: %d\n", ip->ttl);
	printf("		IP PDU Len: %d (bytes)\n", ip->totLen);
	/* determine protocol */
	protocol = getProtocol(ip->protocol);
	/* verify checksum */
	htonIP(ip);
	addr = (unsigned short *)data;
	if (in_cksum(addr, headerLen) == 0) {
		printf("		Checksum: Correct ");
	}
	else {
		printf("		Checksum: Incorrect ");
	}
	printf("(0x%x)\n", ip->checksum);
	ntohIP(ip);
	/* get sender and dest IP */
	printf("		Sender IP: %s\n", inet_ntoa(ip->senderIP));
	if (protocol == 0) {
		printf("		Dest IP: %s", inet_ntoa(ip->destIP));
	}
	else {
		printf("		Dest IP: %s\n\n", inet_ntoa(ip->destIP));
	}
	/* get protocol header */
	htonIP(ip);
	if (protocol == TCP) {
		extractTCP(data + headerLen, ip);
	}
	else if (protocol == UDP) {
		extractUDP(data + headerLen);
	}
	else if (protocol == ICMP) {
		extractICMP(data + headerLen);
	}
	else {
		return;
	}
	return;
}

void extractARP(u_char *data) {
	struct ARPHeader *arp;
	/* get ARP header info */
	printf("\n	ARP header\n");
	arp = (ARPHeader *)data;
	ntohARP(arp);
	/* determine message type */
	if (arp->opCode == ARPRequest) {
		printf("		Opcode: Request\n");
	}
	else if (arp->opCode == ARPReply) {
		printf("		Opcode: Reply\n");
	}
	else {
		printf("		Opcode: improper opcode\n");
		return;
	}
	/* get sender MAC and IP */
	printf("		Sender MAC: %s\n", ether_ntoa(&arp->sourceMAC));
	printf("		Sender IP: %s\n", inet_ntoa(arp->sourceIP));
	/* get target MAC and IP */
	printf("		Target MAC: %s\n", ether_ntoa(&arp->targetMAC));
	printf("		Target IP: %s\n", inet_ntoa(arp->targetIP));
	return;
}

void extractHeaders(u_char *data) {
	struct EtherHeader *eth;
	/* get ethernet header data */
	printf("	Ethernet Header\n");
	eth = (EtherHeader *)data;
	ntohEther(eth);
	printf("		Dest MAC: %s\n", ether_ntoa(&eth->destAddr));
	printf("		Source MAC: %s\n", ether_ntoa(&eth->sourceAddr));
	/* determines type of next header */
	data = (data + startNext);
	if (eth->protocol == ARP) {
		printf("		Type: ARP\n");
		htonEther(eth);
		extractARP(data);
	}
	else if (eth->protocol == IP) {
		printf("		Type: IP\n");
		htonEther(eth);
		extractIP(data);
	}
	else {
		printf("Type: unknown\n");
		htonEther(eth);
	}
	return;
}
