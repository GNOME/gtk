
CC = gcc

dial_test: gtkdial.o dial_test.o
	$(CC) dial_test.o gtkdial.o -o dial_test `gtk-config --libs`

dial_test.o: dial_test.c gtkdial.h
	$(CC) `gtk-config --cflags` -c dial_test.c -o dial_test.o

gtkdial.o: gtkdial.c gtkdial.h
	$(CC) `gtk-config --cflags` -c gtkdial.c -o gtkdial.o

clean: 
	rm -f *.o dial_test
