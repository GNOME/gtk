
CC = gcc

base: base.c 
	$(CC) `gtk-config --cflags`  base.c -o base `gtk-config --libs`

clean: 
	rm -f *.o base
