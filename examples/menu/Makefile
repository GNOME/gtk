
CC = gcc

all: menu itemfactory

menu: menu.c 
	$(CC) `gtk-config --cflags` menu.c -o menu `gtk-config --libs`

itemfactory: itemfactory.c
	$(CC) `gtk-config --cflags` itemfactory.c -o itemfactory `gtk-config --libs`

clean: 
	rm -f *.o menu itemfactory
