
CC = gcc

progressbar: progressbar.c 
	$(CC) `gtk-config --cflags`  progressbar.c -o progressbar `gtk-config --libs`

clean: 
	rm -f *.o progressbar
