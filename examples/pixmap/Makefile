
CC = gcc

pixmap: pixmap.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` pixmap.c -o pixmap

clean: 
	rm -f *.o pixmap
