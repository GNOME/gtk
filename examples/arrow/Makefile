
CC = gcc

arrow: arrow.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  arrow.c -o arrow `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o arrow
