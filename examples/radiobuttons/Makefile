
CC = gcc

radiobuttons: radiobuttons.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  radiobuttons.c -o radiobuttons `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o radiobuttons
