
CC = gcc

clist: clist.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` clist.c -o clist

clean: 
	rm -f *.o clist
