CC = gcc

filesel: filesel.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` filesel.c -o filesel

clean: 
	rm -f *.o filesel
