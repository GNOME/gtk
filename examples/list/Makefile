
CC = gcc

#CFLAGS = -Wall			 	\
#	-DG_DISABLE_DEPRECATED 	 	\
#	-DGDK_DISABLE_DEPRECATED 	\
#	-DGDK_PIXBUF_DISABLE_DEPRECATED \
#	-DGTK_DISABLE_DEPRECATED

CFLAGS = 

list: list.c 
	$(CC) list.c -o list $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f *.o list
