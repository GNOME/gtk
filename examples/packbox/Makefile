
CC = gcc

packbox: packbox.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  packbox.c -o packbox `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o packbox
