
CC = gcc

statusbar: statusbar.c 
	$(CC) `gtk-config --cflags`  statusbar.c -o statusbar `gtk-config --libs`

clean: 
	rm -f *.o statusbar
