
CC = gcc

aspectframe: aspectframe.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  aspectframe.c -o aspectframe `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o aspectframe
