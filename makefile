client: client.c
	gcc client.c -o client

server: server.c
	gcc server.c -o server

all: server client

clean:
	rm -f server client



