
CC = gcc

packer: pack.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  pack.c -o packer `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o packer
