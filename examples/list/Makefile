
CC = gcc

list: list.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` list.c -o list

clean: 
	rm -f *.o list
