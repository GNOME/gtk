
CC = gcc

base: base.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  base.c -o base `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o base
