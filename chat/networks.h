
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/

#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#define BACKLOG 10
#define MAXBUF 1400
#define MAXMESSAGE 200
#define INITCLIENTS 200
#define MAXDESTINATIONS 9
#define TIME_IS_NULL 1
#define TIME_IS_NOT_NULL 2

// for the server side
int tcpServerSetup(int portNumber);

int tcpAccept(int server_socket, fd_set readfds, int debugFlag);

// for the client side
int tcpClientSetup(char * serverName, char * port, int debugFlag);

#endif
