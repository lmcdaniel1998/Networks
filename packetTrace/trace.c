/*---------------------------------------------------*/
/*
 * author: Luke McDaniel
 * institution: California Polytechnic State University
 * course: CPE 464-01 (Networks)
 * assignment: Program 1 (Packet Trace)
*/
/*------------- main.c of trace program -------------*/

/* use pcap library for packet sniffing
 * (must be installed on machine) */
#include <pcap/pcap.h>
/* file I/O and string processing libraries */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* functions for extracting header info */
#include "extractHeader.h"

int main(int argc, char *argv[]) {
	/* buffer for getting error messages */
	char errbuf[PCAP_ERRBUF_SIZE];
	/* pcap structure for passing info */
	pcap_t *pcap;
	/* gets error message from pcap_next_ex */
	int next = 1;
	/* counter for frames */
	int pkt_number = 1;
	/* struct for packet header */
	struct pcap_pkthdr *pkt_header;
	/* pointer to packet data */
	const u_char *pkt_data;
	/* temp structs for memcpy */
	struct pcap_pkthdr *tmp_header = NULL;
	u_char *tmp_data = NULL;

	/* get name of .pcap file passed to program */
	if (argc != 2) {
		fprintf(stderr, "no input .pcap file specified\n");
		return(2);
	}
	/* open .pcap file */
	pcap = pcap_open_offline(argv[1], errbuf);
	if (pcap == NULL) {
		fprintf(stderr, 
		"error opening pcap offline: %s\n", errbuf);
		return(2);
	}

	/* keep getting packets until end of file or error */
	while (next == 1) {
		/* get next packet */
		next = pcap_next_ex(pcap, &pkt_header, &pkt_data);
		if (next == PCAP_ERROR_BREAK) {
			break;
		}
		/* print out packet number and frame length */
		printf("\nPacket number: %d  Frame Len: %d\n\n",
		pkt_number, pkt_header->len);
		/* copy memory from pcap_next_ex */
		tmp_header = realloc(tmp_header, sizeof(struct pcap_pkthdr));
		memcpy(tmp_header, pkt_header, sizeof(struct pcap_pkthdr));
		tmp_data = realloc(tmp_data, pkt_header->len);
		memcpy(tmp_data, pkt_data, pkt_header->len);
		/* extract header info from packet */
		extractHeaders(tmp_data);
		printf("\n");
		pkt_number++;
	}

	/* free all structures */
	free(tmp_header);
	free(tmp_data);
	/* close pcap file */
	pcap_close(pcap);
	return 0;
}
