
CC = gcc

paned: paned.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  paned.c -o paned `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o paned
