
CC = gcc

entry: entry.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  entry.c -o entry `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o entry
