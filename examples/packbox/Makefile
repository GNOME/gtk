
CC = gcc

packbox: packbox.c 
	$(CC) `gtk-config --cflags`  packbox.c -o packbox `gtk-config --libs`

clean: 
	rm -f *.o packbox
