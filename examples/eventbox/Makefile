
CC = gcc

eventbox: eventbox.c 
	$(CC) `gtk-config --cflags`  eventbox.c -o eventbox `gtk-config --libs`

clean: 
	rm -f *.o eventbox
