
CC = gcc

calendar: calendar.c
	$(CC) `pkg-config --cflags gtk+-2.0` calendar.c -o calendar `pkg-config --libs gtk+-2.0`

clean: 
	rm -f calendar
