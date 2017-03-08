#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct Connection Connection;

typedef void (*on_init_t)(Connection* conn);
typedef void (*on_receive_t)(Connection* conn, const uint8_t* payload, size_t bytes);

int start_server(on_init_t, on_receive_t);
int start_client(on_init_t, on_receive_t);
ssize_t send_message(Connection* conn, const uint8_t* buf, size_t bytes);
