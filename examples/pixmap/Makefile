
CC = gcc

pixmap: pixmap.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  pixmap.c -o pixmap `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o pixmap
