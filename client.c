#include "transport.h"
#include "util.h"
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BUFFER_SIZE 4096

void run_get(const char* const local_filename, const char* const remote_filename) {
    // Validate the remote filename
    const size_t remote_filename_len = strlen(remote_filename);
    if (remote_filename_len >= MAX_REMOTE_FILENAME_LEN) { // >= due to \0
        printf("(client) ERROR: remote filename is too long: %s\n", remote_filename);
        exit(1);
    }
    // Open the local filename for write
    FILE* file = fopen(local_filename, "w");
    if (file == 0) {
        printf("(client) ERROR: unable to open file: %s\n", local_filename);
        exit(1);
    }
    // Build the initial message
    uint8_t initial_message[MAX_REMOTE_FILENAME_LEN+7];
    initial_message[0] = 'G';
    initial_message[1] = 'E';
    initial_message[2] = 'T';
    memcpy(initial_message+3, remote_filename, remote_filename_len);
    initial_message[3+remote_filename_len] = '\0';
    // Start the connection
    Connection* conn = start_client();
    if (!conn) {
        puts("(client) ERROR: unable to start the connection");
        exit(1);
    }
    puts("(client) Connection initialized");
    // Send the initial message
    puts("(client) Sending initial message");
    send_message(conn, initial_message, MAX_REMOTE_FILENAME_LEN+7);
    // Read the status response
    uint8_t status_response[MAX_STATUS_MESSAGE_LEN+8];
    receive_message_blocking(conn, status_response, sizeof(status_response));
    const uint32_t status_code = read_uint32(status_response);
    char status_message[MAX_STATUS_MESSAGE_LEN+1];
    memcpy(status_message, status_response+4, MAX_STATUS_MESSAGE_LEN);
    const uint32_t file_length = read_uint32(status_response+204);
    printf("(client) Status code: %u\n", (unsigned) status_code);
    printf("(client) Status message: %s\n", status_message);
    printf("(client) File length: %u\n", (unsigned) file_length);
    if (status_code != 200) {
        puts("(client) Waiting for termination...");
        wait_for_termination(conn);
        fclose(file);
        return;
    }
    // Forward file contents to the local file
    size_t total_bytes = 0;
    uint8_t buffer[BUFFER_SIZE];
    while (total_bytes < file_length) {
        const ssize_t bread = receive_message(conn, buffer, BUFFER_SIZE);
        if (bread < 0) {
            puts("(client) ERROR: receive_message failed!");
            exit(1);
        }
        if (bread == 0) {
            sleep_ms(5);
        } else {
            printf("(client) Read %d bytes\n", (int) bread);
            fwrite(buffer, 1, bread, file);
            total_bytes += bread;
        }
    }
    // Exit
    fclose(file);
    wait_for_termination(conn);
}

void run_put(const char* const local_filename, const char* const remote_filename) {
    // Validate the remote filename
    const size_t remote_filename_len = strlen(remote_filename);
    if (remote_filename_len >= MAX_REMOTE_FILENAME_LEN) { // >= due to \0
        printf("(client) ERROR: remote filename is too long: %s\n", remote_filename);
        exit(1);
    }
    // Open the local filename for read
    FILE* file = fopen(local_filename, "r");
    if (file == 0) {
        printf("unable to open file: %s\n", local_filename);
        exit(1);
    }
    // Build the initial message
    fseek(file, 0, SEEK_END);
    const uint32_t file_length = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t initial_message[MAX_REMOTE_FILENAME_LEN+7];
    initial_message[0] = 'P';
    initial_message[1] = 'U';
    initial_message[2] = 'T';
    memcpy(initial_message+3, remote_filename, remote_filename_len);
    initial_message[3+remote_filename_len] = '\0';
    write_uint32(initial_message+203, file_length);
    // Start the connection
    Connection* conn = start_client();
    if (!conn) {
        puts("(client) ERROR: unable to start the connection");
        exit(1);
    }
    puts("(client) Connection initialized");
    // Send the initial message
    puts("(client) Sending initial message");
    send_message(conn, initial_message, MAX_REMOTE_FILENAME_LEN+7);
    // Read the status response
    uint8_t status_response[MAX_STATUS_MESSAGE_LEN+8];
    receive_message_blocking(conn, status_response, sizeof(status_response));
    const uint32_t status_code = read_uint32(status_response);
    char status_message[MAX_STATUS_MESSAGE_LEN+1];
    memcpy(status_message, status_response+4, MAX_STATUS_MESSAGE_LEN);
    printf("(client) Status code: %u\n", (unsigned) status_code);
    printf("(client) Status message: %s\n", status_message);
    if (status_code != 200) {
        puts("(client) Waiting for termination...");
        wait_for_termination(conn);
        fclose(file);
        return;
    }
    // Forward file contents to the server
    uint8_t buffer[BUFFER_SIZE];
    for (;;) {
        size_t bread = fread(buffer, 1, BUFFER_SIZE, file);
        if (bread == 0) {
            puts("(server) Get the last block, terminating the connection...");
            terminate_connection(conn);
            return;
        }
        puts("(server) Geting a new block");
        send_message_blocking(conn, buffer, bread);
    }
    // Exit
    fclose(file);
    wait_for_termination(conn);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("(client) ERROR: expected 3 arguments, received %d\n", argc-1);
        return 1;
    }
    if (strcmp(argv[1], "GET") == 0) {
        run_get(argv[2], argv[3]);
    } else if (strcmp(argv[1], "PUT") == 0) {
        run_put(argv[2], argv[3]);
    } else {
        printf("(client) ERROR: invalid method %s, expected GET or PUT\n", argv[1]);
        return -1;
    }
    return 0;
}
