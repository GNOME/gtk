
CC = gcc

scrolledwin: scrolledwin.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  scrolledwin.c -o scrolledwin `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o scrolledwin
