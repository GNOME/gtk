
CC = gcc

entry: entry.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` entry.c -o entry

clean: 
	rm -f *.o entry
