
CC = gcc

helloworld: helloworld.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  helloworld.c -o helloworld `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o helloworld
