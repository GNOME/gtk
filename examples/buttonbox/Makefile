
CC = gcc

buttonbox: buttonbox.c 
	$(CC) `gtk-config --cflags`  buttonbox.c -o buttonbox `gtk-config --libs`

clean: 
	rm -f *.o buttonbox
