
CC = gcc

rulers: rulers.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  rulers.c -o rulers `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o rulers
