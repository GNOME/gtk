
CC = gcc

text: text.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  text.c -o text `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o text
