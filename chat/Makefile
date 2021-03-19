# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall
LIBS = 


all:   client server

client: cclient.c networks.o gethostbyname6.o sharedFuncs.o
	$(CC) $(CFLAGS) -o cclient cclient.c networks.o gethostbyname6.o sharedFuncs.o $(LIBS)

server: server.c networks.o gethostbyname6.o sharedFuncs.o linkedlist.o
	$(CC) $(CFLAGS) -o server server.c networks.o gethostbyname6.o sharedFuncs.o linkedlist.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o




