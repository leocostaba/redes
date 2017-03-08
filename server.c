#include "transport.h"
#include <stdio.h>

void on_init(Connection* const conn) {
    puts("(server) Connection initialized");
}

void on_receive(Connection* const conn, const uint8_t* const payload, const size_t bytes) {
    printf("(server) Received %u bytes: ", bytes);
    for (int i = 0; i < bytes; ++i)
        putchar((char) payload[i]);
    puts("");
    printf("(server) Sending a response\n");
    send_message(conn, "recebi", 6);
    /*printf("x = %d\n", (int) *payload);*/
}

int main() {
    start_server(on_init, on_receive);
    return 0;
}
