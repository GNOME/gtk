
CC = gcc

all: menu menufactory

menu: menu.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` menu.c -o menu

menufactory: menufactory.o mfmain.o
	$(CC) `gtk-config --libs` menufactory.o mfmain.o -o menufactory

menufactory.o: menufactory.c mfmain.h
	$(CC) `gtk-config --cflags` -c menufactory.c -o menufactory.o

mfmain.o: mfmain.c mfmain.h menufactory.h
	$(CC) `gtk-config --cflags` -c mfmain.c -o mfmain.o

clean: 
	rm -f *.o menu menufactory
