
CC = gcc

buttons: buttons.c 
	$(CC) `gtk-config --cflags`  buttons.c -o buttons `gtk-config --libs`

clean: 
	rm -f *.o buttons
