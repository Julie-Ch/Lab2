CC=gcc
CFLAGS=-g -O -Wall -std=c11
LDLIBS=-lm -lrt -pthread

EXEC=archivio.out

all: $(EXEC)

%.out: %.o xerrori.o hashtable.o
	$(CC) $(LDLIBS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(EXEC)
