
CC = gcc

packer: packer.c 
	$(CC) `gtk-config --cflags`  pack.c -o packer `gtk-config --libs`

clean: 
	rm -f *.o packer
