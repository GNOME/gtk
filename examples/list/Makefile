
CC = gcc

list: list.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  list.c -o list `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o list
