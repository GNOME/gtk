
CC = gcc

all: gettargets setselection

gettargets: gettargets.c 
	$(CC) `pkg-config --cflags gtk+-2.0` gettargets.c -o gettargets `pkg-config --libs gtk+-2.0`

setselection: setselection.c
	$(CC) `pkg-config --cflags gtk+-2.0` setselection.c -o setselection `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o gettargets setselection
