
CC = gcc

ttt_test: tictactoe.o ttt_test.o
	$(CC) ttt_test.o tictactoe.o -o ttt_test `pkg-config --libs gtk+-2.0`

ttt_test.o: ttt_test.c tictactoe.h
	$(CC) `pkg-config --cflags gtk+-2.0` -c ttt_test.c -o ttt_test.o

tictactoe.o: tictactoe.c tictactoe.h
	$(CC) `pkg-config --cflags gtk+-2.0` -c tictactoe.c -o tictactoe.o

clean: 
	rm -f *.o ttt_test
