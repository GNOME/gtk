
CC = gcc

progressbar: progressbar.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  progressbar.c -o progressbar `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o progressbar
