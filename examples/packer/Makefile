
CC = gcc

packer: pack.c 
	$(CC) -g `gtk-config --cflags`  pack.c -o packer `gtk-config --libs`

clean: 
	rm -f *.o packer
