
CC = gcc

notebook: notebook.c 
	$(CC) `gtk-config --cflags`  notebook.c -o notebook `gtk-config --libs`

clean: 
	rm -f *.o notebook
