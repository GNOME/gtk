
CC = gcc

helloworld: helloworld.c 
	$(CC) `gtk-config --cflags`  helloworld.c -o helloworld `gtk-config --libs`

clean: 
	rm -f *.o helloworld
