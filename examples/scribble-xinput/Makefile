
CC = gcc

scribble-xinput: scribble-xinput.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  scribble-xinput.c -o scribble-xinput `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o scribble-xinput
