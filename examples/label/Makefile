
CC = gcc

label: label.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  label.c -o label `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o label
