
CC = gcc

eventbox: eventbox.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  eventbox.c -o eventbox `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o eventbox
