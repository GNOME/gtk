
CC = gcc

buttons: buttons.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  buttons.c -o buttons `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o buttons
