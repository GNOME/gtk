
CC = gcc

buttonbox: buttonbox.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  buttonbox.c -o buttonbox `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o buttonbox
