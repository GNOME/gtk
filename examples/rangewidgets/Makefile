
CC = gcc

rangewidgets: rangewidgets.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  rangewidgets.c -o rangewidgets `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o rangewidgets
