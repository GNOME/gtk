
CC = gcc

wheelbarrow: wheelbarrow.c 
	$(CC) `gtk-config --cflags` wheelbarrow.c -o wheelbarrow `gtk-config --libs`

clean: 
	rm -f *.o wheelbarrow
