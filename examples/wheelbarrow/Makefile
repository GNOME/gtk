
CC = gcc

wheelbarrow: wheelbarrow.c 
	$(CC) `pkg-config --cflags gtk+-2.0` wheelbarrow.c -o wheelbarrow `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o wheelbarrow
