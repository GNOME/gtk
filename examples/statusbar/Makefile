
CC = gcc

statusbar: statusbar.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` statusbar.c -o statusbar

clean: 
	rm -f *.o statusbar
