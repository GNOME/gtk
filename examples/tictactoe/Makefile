
CC = gcc

CFLAGS = -Wall			 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

ttt_test: tictactoe.o ttt_test.o
	$(CC) ttt_test.o tictactoe.o -o ttt_test `pkg-config --libs gtk+-2.0`

ttt_test.o: ttt_test.c tictactoe.h
	$(CC) -c ttt_test.c -o ttt_test.o $(CFLAGS) `pkg-config gtk+-2.0 --cflags`

tictactoe.o: tictactoe.c tictactoe.h
	$(CC) -c tictactoe.c -o tictactoe.o $(CFLAGS) `pkg-config gtk+-2.0 --cflags`

clean: 
	rm -f *.o ttt_test
