
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

all: gettargets setselection

gettargets: gettargets.c 
	$(CC) gettargets.c -o gettargets $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`

setselection: setselection.c
	$(CC) setselection.c -o setselection $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f *.o gettargets setselection
