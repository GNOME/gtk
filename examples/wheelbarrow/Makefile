
CC = gcc

wheelbarrow: wheelbarrow.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` wheelbarrow.c -o wheelbarrow

clean: 
	rm -f *.o wheelbarrow
