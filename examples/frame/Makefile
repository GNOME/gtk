
CC = gcc

frame: frame.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  frame.c -o frame `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o frame
