
CC = gcc

rulers: rulers.c 
	$(CC) `gtk-config --cflags`  rulers.c -o rulers `gtk-config --libs`

clean: 
	rm -f *.o rulers
