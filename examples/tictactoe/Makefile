
CC = gcc

ttt_test: tictactoe.o ttt_test.o
	$(CC) `gtk-config --libs` ttt_test.o tictactoe.o -o ttt_test

ttt_test.o: 
	$(CC) `gtk-config --cflags` -c ttt_test.c -o ttt_test.o

tictactoe.o: tictactoe.c tictactoe.h
	$(CC) `gtk-config --cflags` -c tictactoe.c -o tictactoe.o

clean: 
	rm *.o ttt_test
