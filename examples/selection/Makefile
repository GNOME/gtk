
CC = gcc

all: gettargets setselection

gettargets: gettargets.c 
	$(CC) `gtk-config --cflags` gettargets.c -o gettargets `gtk-config --libs`

setselection: setselection.c
	$(CC) `gtk-config --cflags` setselection.c -o setselection `gtk-config --libs`
 
clean: 
	rm -f *.o gettargets setselection
