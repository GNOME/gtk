
CC = gcc

buttons: buttons.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` buttons.c -o buttons

clean: 
	rm -f *.o buttons
