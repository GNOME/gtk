
CC = gcc

table: table.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  table.c -o table `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o table
