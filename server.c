#include "transport.h"
#include <stdio.h>
#define MAX_REMOTE_FILENAME_LEN 200
#define MAX_LOCAL_FILENAME_LEN 200
#define MAX_STATUS_MESSAGE_LEN 200
#define BUFFER_SIZE 4096

void on_init(Connection* const conn) {
    puts("(server) Connection initialized");
}

void on_receive(Connection* const conn, const uint8_t* const payload, const size_t bytes) {
    if (payload[0] == 'G' && payload[1] == 'E' && payload[2] == 'T') {
        char filename[MAX_REMOTE_FILENAME_LEN+1];
        memcpy(filename, payload+3, MAX_REMOTE_FILENAME_LEN);
        FILE* file = fopen(filename, "r");
        if (file == 0) {
            char response[MAX_STATUS_MESSAGE_LEN+4];
            write_uint32(response, 404);
            strcpy(response+4, "Not Found");
            send_message(conn, response, MAX_STATUS_MESSAGE_LEN+4);
            //TODO: terminate connection
            return;
        }
        char response[MAX_STATUS_MESSAGE_LEN+4];
        write_uint32(response, 200);
        strcpy(response+4, "OK");
        send_message(conn, response, MAX_STATUS_MESSAGE_LEN+4);
        char buffer[BUFFER_SIZE];
        for (;;) {
            size_t bread = fread(buffer, 1, BUFFER_SIZE, file);
            if (bread == 0) {
                //TODO: terminate connection
                return;
            }
            send_message(conn, buffer, bread);
            //TODO: wait if the "send" fails
        }
    } else if (payload[0] == 'P' && payload[1] == 'U' && payload[2] == 'T') {
        char filename[MAX_LOCAL_FILENAME_LEN+1];
        memcpy(filename, payload+3, MAX_LOCAL_FILENAME_LEN);
        FILE* file = fopen(filename, "r");
        if (file == 0) {
            char response[MAX_STATUS_MESSAGE_LEN+4];
            write_uint32(response, 404);
            strcpy(response+4, "Not Found");
            send_message(conn, response, MAX_STATUS_MESSAGE_LEN+4);
            //TODO: terminate connection
            return;
        }
        char response[MAX_STATUS_MESSAGE_LEN+4];
        write_uint32(response, 200);
        strcpy(response+4, "OK");
        send_message(conn, response, MAX_STATUS_MESSAGE_LEN+4);
        char buffer[BUFFER_SIZE];
        for (;;) {
            size_t bread = fread(buffer, 1, BUFFER_SIZE, file);
            if (bread == 0) {
                //TODO: terminate connection
                return;
            }
            send_message(conn, buffer, bread);
            //TODO: wait if the "send" fails
        }
    } else {
        puts("WARNING: invalid method");
    }
}

int main() {
    start_server(on_init, on_receive);
    return 0;
}
