
CC = gcc

scribble-simple: scribble-simple.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  scribble-simple.c -o scribble-simple `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o scribble-simple
