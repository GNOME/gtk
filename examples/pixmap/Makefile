
CC = gcc

pixmap: pixmap.c 
	$(CC) `gtk-config --cflags`  pixmap.c -o pixmap `gtk-config --libs`

clean: 
	rm -f *.o pixmap
