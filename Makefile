all: client server

client: constants.h transport.h transport.c util.h util.c client.c
	gcc -g -O2 -pthread -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal transport.c util.c client.c -o client

server: constants.h transport.h transport.c util.h util.c server.c
	gcc -g -O2 -pthread -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal transport.c util.c server.c -o server

clean:
	rm client server
