
CC = gcc

list: list.c 
	$(CC) `gtk-config --cflags`  list.c -o list `gtk-config --libs`

clean: 
	rm -f *.o list
