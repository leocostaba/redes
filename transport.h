#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct Connection Connection;

Connection* start_transport_server(const uint32_t local_address, const uint32_t remote_address);
Connection* start_transport_client(const uint32_t local_address, const uint32_t remote_address);
ssize_t send_message(Connection* conn, const uint8_t* buf, size_t bytes);
ssize_t receive_message(Connection* conn, uint8_t* buf, size_t bytes);
void terminate_connection(Connection* conn);
void wait_for_termination(Connection* conn);

void send_message_blocking(Connection* conn, const uint8_t* buf, size_t bytes);
void receive_message_blocking(Connection* conn, uint8_t* buf, size_t bytes);
