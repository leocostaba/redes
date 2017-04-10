all: client server

client: constants.h transport.h network.h link.h util.h transport.c util.c client.c network.c link.c link_receiver.c link_transmitter.c
	gcc -g -O2 -pthread -lrt -lasound -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal client.c transport.c network.c link.c link_receiver.c link_transmitter.c util.c -o client -lm -lportaudio 

server: constants.h transport.h network.h link.h util.h transport.c util.c server.c network.c link.c link_receiver.c link_transmitter.c
	gcc -g -O2 -pthread -lrt -lasound -std=gnu11 -Wall -Wunused-function -Wunused-parameter -Wunused-variable -Wfloat-equal transport.c util.c server.c -o server network.c link.c link_receiver.c link_transmitter.c -lm -lportaudio

clean:
	rm client server
