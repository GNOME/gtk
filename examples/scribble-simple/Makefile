
CC = gcc

scribble-simple: scribble-simple.c 
	$(CC) `gtk-config --cflags`  scribble-simple.c -o scribble-simple `gtk-config --libs`

clean: 
	rm -f *.o scribble-simple
