
CC = gcc

helloworld2: helloworld2.c 
	$(CC) `pkg-config --cflags gtk+-2.0`  helloworld2.c -o helloworld2 `pkg-config --libs gtk+-2.0`

clean: 
	rm -f *.o helloworld2
