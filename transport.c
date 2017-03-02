#include "transport.h"
#include "util.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

struct Connection {
    int8_t type;
    int readpipe, writepipe;
};

enum {
    CONNECTION_TYPE_SERVER = 0,
    CONNECTION_TYPE_CLIENT = 1,
};

const size_t DATAGRAM_SIZE = 1<<16;
const size_t HEADER_SIZE = 4;
const char* const filename_servertoclient = "/tmp/trabalho-redes-transporte-stc";
const char* const filename_clienttoserver = "/tmp/trabalho-redes-transporte-cts";

int start_server(Connection* const conn) {
    // TODO: remove previously created files?
    if (mkfifo(filename_servertoclient, 0666) != 0)
        return -1;
    if (mkfifo(filename_clienttoserver, 0666) != 0)
        return -1;
    const int readpipe = open(filename_servertoclient, O_RDONLY);
    if (readpipe == -1)
        return -1;
    const int writepipe = open(filename_clienttoserver, O_WRONLY);
    if (writepipe == -1) {
        close(readpipe);
        return -1;
    }
    conn->type = CONNECTION_TYPE_SERVER;
    conn->readpipe = readpipe;
    conn->writepipe = writepipe;
    return 0;
}

int start_client(Connection* const conn) {
    if (mkfifo(filename_clienttoserver, 0666) != 0)
        return -1;
    if (mkfifo(filename_servertoclient, 0666) != 0)
        return -1;
    const int readpipe = open(filename_clienttoserver, O_RDONLY);
    if (readpipe == -1)
        return -1;
    const int writepipe = open(filename_servertoclient, O_WRONLY);
    if (writepipe == -1) {
        close(readpipe);
        return -1;
    }
    conn->type = CONNECTION_TYPE_SERVER;
    conn->readpipe = readpipe;
    conn->writepipe = writepipe;
    return 0;
}

int send_datagram(Connection* const conn, const uint8_t* const buf) {
    const ssize_t written = write(conn->writepipe, buf, DATAGRAM_SIZE);
    if (written == -1)
        exit(1);
    if ((size_t) written < DATAGRAM_SIZE)
        exit(2);
    return 0;
}

int receive_datagram(Connection* const conn, uint8_t* const buf) {
    const ssize_t bread = read(conn->readpipe, buf, DATAGRAM_SIZE);
    if (bread == -1)
        exit(1);
    if ((size_t) bread < DATAGRAM_SIZE)
        exit(2);
    return 0;
}

// Initial version assuming that no datagrams are lost
#if 0
int send_message(Connection* const conn, char* const payload, const size_t bytes) {
    const size_t PAYLOAD_MAX_SIZE = DATAGRAM_SIZE - HEADER_SIZE;
    const int required_datagrams = (bytes / PAYLOAD_MAX_SIZE) + (bytes % PAYLOAD_MAX_SIZE != 0);
    uint8_t buf[DATAGRAM_SIZE];
    for (int datagram = 0; datagram < required_datagrams; ++datagram) {
        const char* const payload_begin = payload + datagram*PAYLOAD_MAX_SIZE;
    }
}
#endif
