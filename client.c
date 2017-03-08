#include "transport.h"
#include <stdio.h>

void on_init(Connection* const conn) {
    puts("(client) Connection initialized");
    send_message(conn, "olamundo", 5);
}

void on_receive(Connection* const conn, const uint8_t* const payload, const size_t bytes) {
    printf("(client) Received %u bytes: ", bytes);
    fwrite(payload, 1, bytes, stdin);
}

int main() {
    const int ret = start_client(on_init, on_receive);
    printf("(client) Connection closed: %d\n", ret);
    return 0;
}
