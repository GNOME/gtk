
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED

all: menu

menu: menu.c 
	$(CC) menu.c -o menu $(CFLAGS) `pkg-config gtk+-3.0 --cflags --libs`

clean: 
	rm -f *.o menu
