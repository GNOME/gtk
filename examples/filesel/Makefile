
CC = gcc

filesel: filesel.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  filesel.c -o filesel `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o filesel
