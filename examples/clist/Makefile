
CC = gcc

clist: clist.c 
	$(CC) `gtk-config --cflags`  clist.c -o clist `gtk-config --libs`

clean: 
	rm -f *.o clist
