#include "transport.h"
#include "util.h"
#include "constants.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define BUFFER_SIZE 4096

void run_get(Connection* const conn, const uint8_t* const message) {
    // Open the file for read access
    char filename[MAX_REMOTE_FILENAME_LEN+1];
    memcpy(filename, message+3, MAX_REMOTE_FILENAME_LEN);
    printf("(server) Opening file for read access: %s\n", filename);
    FILE* file = fopen(filename, "r");
    // If it fails, reply with a failure message and terminate the connection
    if (file == 0) {
        puts("(server) Unable to open the file: replying with failure");
        uint8_t response[MAX_STATUS_MESSAGE_LEN+8];
        write_uint32(response, 404);
        strcpy((char*) response+4, "Not Found");
        send_message_blocking(conn, response, MAX_STATUS_MESSAGE_LEN+8);
        puts("(server) Terminating the connection");
        terminate_connection(conn);
        return;
    }
    // If it succeeds, reply with a success message containing the file length
    fseek(file, 0, SEEK_END);
    const uint32_t file_length = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t response[MAX_STATUS_MESSAGE_LEN+8];
    puts("(server) Success opening the file: replying with the file length");
    write_uint32(response, 200);
    strcpy((char*) response+4, "OK");
    write_uint32(response+204, file_length);
    send_message_blocking(conn, response, MAX_STATUS_MESSAGE_LEN+8);
    // And then send the file contents
    puts("(server) Sending file contents...");
    uint8_t buffer[BUFFER_SIZE];
    for (;;) {
        size_t bread = fread(buffer, 1, BUFFER_SIZE, file);
        if (bread == 0) {
            puts("(server) Sent the last block, terminating the connection...");
            terminate_connection(conn);
            return;
        }
        puts("(server) Sending a new block");
        send_message_blocking(conn, buffer, bread);
    }
}

void run_put(Connection* const conn, const uint8_t* const message) {
    //TODO: implement this function
}

int main() {
    for (;;) {
        puts("======================================================");
        puts("(server) Starting server...");
        Connection* conn = start_server();
        puts("(server) Connection initialized");
        // Read the first message
        uint8_t message[MAX_REMOTE_FILENAME_LEN+7];
        receive_message_blocking(conn, message, sizeof(message));
        // Call the corresponding function according to the method
        if (message[0] == 'G' && message[1] == 'E' && message[2] == 'T') {
            run_get(conn, message);
        } else if (message[0] == 'P' && message[1] == 'U' && message[2] == 'T') {
            run_put(conn, message);
        } else {
            puts("(server) WARNING: invalid method");
        }
        // Wait a miliseconds for the pipe to close (won't be necessary after replacing the network layer)
        sleep_ms(500);
    }
    // Return value
    return 0;
}
