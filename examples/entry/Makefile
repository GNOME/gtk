
CC = gcc

entry: entry.c 
	$(CC) `gtk-config --cflags`  entry.c -o entry `gtk-config --libs`

clean: 
	rm -f *.o entry
