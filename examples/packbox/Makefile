
CC = gcc

packbox: packbox.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` packbox.c -o packbox

clean: 
	rm -f *.o packbox
