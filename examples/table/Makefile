
CC = gcc

table: table.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` table.c -o table

clean: 
	rm -f *.o table
