
CC = gcc

eventbox: eventbox.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` eventbox.c -o eventbox

clean: 
	rm -f *.o eventbox
