CC=gcc
CFLAGS=-c -Wall

all: main

main: main.o SLList.o
	$(CC) main.o SLList.o -o main

main.o: main.c
	$(CC) $(CFLAGS) main.c

SLList.o: SLList.c
	$(CC) $(CFLAGS) SLList.c

clean:
		rm -rf *.o
