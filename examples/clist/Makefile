
CC = gcc

clist: clist.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  clist.c -o clist `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o clist
