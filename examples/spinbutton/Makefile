
CC = gcc

spinbutton: spinbutton.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  spinbutton.c -o spinbutton `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o spinbutton
