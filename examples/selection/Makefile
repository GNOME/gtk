
CC = gcc

all: gettargets setselection

gettargets: gettargets.c 
	$(CC) `gtk-config --cflags` `gtk-config --libs` gettargets.c -o gettargets

setselection: setselection.c
	$(CC) `gtk-config --cflags` `gtk-config --libs` setselection.c -o setselection
 
clean: 
	rm -f *.o gettargets setselection
