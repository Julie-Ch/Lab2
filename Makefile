CC=gcc
CFLAGS=-g -O -Wall -std=c11
LDLIBS=-lm -lrt -pthread

EXEC=Archivio.out

all: $(EXEC)

# regola per la creazioni degli eseguibili utilizzando xerrori.o e hashtable.o
%.out: %.o xerrori.o hashtable.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# regola per la creazione di file oggetto che dipendono da xerrori.h e hashtable.h
%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(EXEC)
