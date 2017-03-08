#include "transport.h"
#include <stdio.h>
#include <string.h>
#define MAX_REMOTE_FILENAME_LEN 200
#define MAX_LOCAL_FILENAME_LEN 200
#define MAX_STATUS_MESSAGE_LEN 200
//TODO: add thread

FILE* file;
uint8_t initial_message[MAX_REMOTE_FILENAME_LEN+7];

void on_init(Connection* const conn) {
    puts("(client) Connection initialized");
    send_message(conn, initial_message, MAX_REMOTE_FILENAME_LEN+7);
}

//TODO: improve the transport layer (fixed number of expected bytes)
uint8_t first_message = 1;
void on_receive(Connection* const conn, const uint8_t* const payload, const size_t bytes) {
    puts("asd");
    if (first_message) {
        first_message = 0;
        const uint32_t status_code = read_uint32(payload);
        char status_message[MAX_STATUS_MESSAGE_LEN+1];
        memcpy(status_message, payload+4, MAX_STATUS_MESSAGE_LEN);
        printf("(client) Status code: %d\n", (int) status_code);
        printf("(client) Status message: %s\n", status_message);
        if (status_code != 200) {
            puts("(client) Exiting...");
            //TODO: finish connection
            return;
        }
    } else {
        printf("(client) Received %u bytes\n", bytes);
        fwrite(payload, 1, bytes, file);
    }
}

int run_get(const char* const local_filename, const char* const remote_filename) {
    const size_t remote_filename_len = strlen(remote_filename);
    if (remote_filename_len > MAX_REMOTE_FILENAME_LEN) {
        printf("remote filename is too long: %s\n", remote_filename);
        return 1;
    }
    file = fopen(local_filename, "w");
    if (file == 0) {
        printf("unable to open file: %s\n", local_filename);
        return 1;
    }
    initial_message[0] = 'G';
    initial_message[1] = 'E';
    initial_message[2] = 'T';
    memcpy(initial_message+3, remote_filename, remote_filename_len);
    initial_message[3+remote_filename_len] = '\0';
    const int ret = start_client(on_init, on_receive);
    return ret;
}

int run_put(const char* const local_filename, const char* const remote_filename) {
   const size_t local_filename_len = strlen(local_filename);
    if (local_filename_len > MAX_LOCAL_FILENAME_LEN) {
        printf("local filename is too long: %s\n", local_filename);
        return 1;
    }
    file = fopen(remote_filename, "r");
    if (file == 0) {
        printf("unable to open file: %s\n", remote_filename);
        return 1;
    }
    initial_message[0] = 'P';
    initial_message[1] = 'U';
    initial_message[2] = 'T';
    memcpy(initial_message+3, local_filename, local_filename_len);
    initial_message[3+local_filename_len] = '\0';
    const int ret = start_client(on_init, on_receive);
    return ret;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("expected 3 arguments, received %d\n", argc-1);
        return 1;
    }
    if (strcmp(argv[1], "GET") == 0) {
        return run_get(argv[2], argv[3]);
    } else if (strcmp(argv[1], "PUT") == 0) {
        return run_put(argv[2], argv[3]);
    } else {
        printf("invalid method %s, expected GET or PUT\n", argv[1]);
        return -1;
    }

    return 0;
    const int ret = start_client(on_init, on_receive);
    printf("(client) Connection closed: %d\n", ret);
    return 0;
}
