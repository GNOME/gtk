
CC = gcc

tree: tree.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  tree.c -o tree `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o tree
