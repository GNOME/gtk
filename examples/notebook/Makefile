
CC = gcc

notebook: notebook.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  notebook.c -o notebook `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o notebook
