
CC = gcc

tree: tree.c 
	$(CC) `gtk-config --cflags`  tree.c -o tree `gtk-config --libs`

clean: 
	rm -f *.o tree
