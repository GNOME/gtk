
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

arrow: arrow.c 
	$(CC) arrow.c -o arrow $(DEPRECTATED) `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f *.o arrow
