
CC = gcc

all: menu itemfactory

menu: menu.c 
	$(CC) `pkg-config --cflags gtk+-2.0` menu.c -o menu `pkg-config --libs gtk+-2.0`

itemfactory: itemfactory.c
	$(CC) `pkg-config --cflags gtk+-2.0` itemfactory.c -o itemfactory `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o menu itemfactory
