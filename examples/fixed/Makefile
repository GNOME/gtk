
CC = gcc

fixed: fixed.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  fixed.c -o fixed `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o fixed
