
CC = gcc

table: table.c 
	$(CC) `gtk-config --cflags`  table.c -o table `gtk-config --libs`

clean: 
	rm -f *.o table
