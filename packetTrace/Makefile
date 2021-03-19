TARGET	 = trace
CC 	 = gcc
CFLAGS 	 = -g -Wall 
SOURCES	 = $(wildcard *.c)
INCLUDES = $(wildcard *.h)
OBJECTS	 = $(SOURCES:.c=.o)

all:$(TARGET)

#all:  trace

#trace: trace.c
#	$(CC) $(CFLAGS) -o trace trace.c checksum.c  -lpcap 

$(TARGET):$(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) -lpcap

$(OBJECTS):$(SOURCES) $(INCLUDES)
	$(CC) -c $(CFLAGS) $(SOURCES)

clean:
	rm *.o
	rm -f trace
