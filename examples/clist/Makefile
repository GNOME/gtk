
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

clist: clist.c 
	$(CC) clist.c -o clist `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f *.o clist
