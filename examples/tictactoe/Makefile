
CC = gcc

ttt_test: tictactoe.o ttt_test.o
	$(CC) ttt_test.o tictactoe.o -o ttt_test `gtk-config --libs`

ttt_test.o: ttt_test.c tictactoe.h
	$(CC) `gtk-config --cflags` -c ttt_test.c -o ttt_test.o

tictactoe.o: tictactoe.c tictactoe.h
	$(CC) `gtk-config --cflags` -c tictactoe.c -o tictactoe.o

clean: 
	rm -f *.o ttt_test
