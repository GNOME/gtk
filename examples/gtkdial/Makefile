
CC = gcc

dial_test: gtkdial.o dial_test.o
	$(CC) dial_test.o gtkdial.o -o dial_test `pkg-config --libs gtk+-2.0`

dial_test.o: dial_test.c gtkdial.h
	$(CC) `pkg-config --cflags gtk+-2.0` -c dial_test.c -o dial_test.o

gtkdial.o: gtkdial.c gtkdial.h
	$(CC) `pkg-config --cflags gtk+-2.0` -c gtkdial.c -o gtkdial.o

clean: 
	rm -f *.o dial_test
