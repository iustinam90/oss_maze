CFLAGS = -Wall -g

all: mshell piece

mshell: 
	gcc -o mshell mshell.c
piece: 
	gcc -o piece piece.c

.PHONY: all clean

clean:
	rm -f mshell piece  
