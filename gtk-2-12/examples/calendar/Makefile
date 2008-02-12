
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED

calendar: calendar.c
	$(CC) calendar.c -o calendar $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`

clean: 
	rm -f calendar
