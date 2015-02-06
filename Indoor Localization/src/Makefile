CFLAGS= -g
LDFLAGS=# -lsocket -lnsl
CC=g++

all: Node
 
Node: Node.o
	$(CC) $(LDFLAGS) -o Node Node.o
	
# To make an object from source
.c.o:
	$(CC) $(CFLAGS) -c $*.c

# clean out the dross
clean:
	-rm Node *.o core

