
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

dial_test: gtkdial.o dial_test.o
	$(CC) dial_test.o gtkdial.o -o dial_test `pkg-config --libs gtk+-2.0`

dial_test.o: dial_test.c gtkdial.h
	$(CC) -c dial_test.c -o dial_test.o $(CFLAGS) `pkg-config gtk+-2.0 --cflags`

gtkdial.o: gtkdial.c gtkdial.h
	$(CC) -c gtkdial.c -o gtkdial.o $(CFLAGS) `pkg-config gtk+-2.0 --cflags`

clean: 
	rm -f *.o dial_test
