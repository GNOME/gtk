
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED

scribble-xinput: scribble-xinput.c 
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-2.0` scribble-xinput.c -o scribble-xinput $(LDFLAGS) `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o scribble-xinput
