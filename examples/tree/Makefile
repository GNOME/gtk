
CC = gcc

tree: tree.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` tree.c -o tree

clean: 
	rm -f *.o tree
