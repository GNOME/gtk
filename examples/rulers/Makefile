
CC = gcc

rulers: rulers.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` rulers.c -o rulers

clean: 
	rm -f *.o rulers
