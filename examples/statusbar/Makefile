
CC = gcc

statusbar: statusbar.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  statusbar.c -o statusbar `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o statusbar
