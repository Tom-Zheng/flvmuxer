CC=gcc
CFLAGS=-I.
unitTest: unitTest.o flv.o
	$(CC) -o unitTest unitTest.o flv.o

