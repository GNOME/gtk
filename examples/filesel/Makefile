
CC = gcc

filesel: filesel.c 
	$(CC) `gtk-config --cflags`  filesel.c -o filesel `gtk-config --libs`

clean: 
	rm -f *.o filesel
