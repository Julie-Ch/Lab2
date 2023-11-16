CC=gcc
CFLAGS=-g -O -Wall -std=c99
LIBS=-lm -lrt -pthread
INCLUDES=xerrori.h hashtable.h
TARGET=Archivio
OBJS=Archivio.o xerrori.o hashtable.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

Archivio.o: Archivio.c $(INCLUDES)
	$(CC) $(CFLAGS) -c Archivio.c

xerrori.o: xerrori.c xerrori.h
	$(CC) $(CFLAGS) -c xerrori.c

hashtable.o: hashtable.c hashtable.h
	$(CC) $(CFLAGS) -c hashtable.c

clean:
	rm -f *.o $(TARGET)
