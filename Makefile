all: client server

client: transport.h transport.c util.h util.c client.c
	gcc -g -O2 -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal transport.c util.c client.c -o client

server: transport.h transport.c util.h util.c server.c
	gcc -g -O2 -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal transport.c util.c server.c -o server

clean:
	rm client server