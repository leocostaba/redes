#include "transport.h"
#include "network.h"
#include "util.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#define GO_BACK 2 // should be a power of two to correctly handle unsigned overflow in the ring buffer
#define HEADER_SIZE 16
#define MAX_PAYLOAD_SIZE 76
#define SEGMENT_SIZE (HEADER_SIZE+MAX_PAYLOAD_SIZE)
#define BUFFER_SIZE (1<<16) // should be a power of two to correctly handle unsigned overflow in the ring buffer

const uint32_t SEGMENT_TYPE_INIT = 0x11213141;
const uint32_t SEGMENT_TYPE_INIT_ACK = 0x12223242;
const uint32_t SEGMENT_TYPE_MESSAGE = 0x13233343;
const uint32_t SEGMENT_TYPE_MESSAGE_ACK = 0x14243444;
const uint32_t SEGMENT_TYPE_BYE = 0x15253545;
const uint32_t SEGMENT_TYPE_BYE_ACK = 0x16263646;

const uint8_t SERVER_STATUS_DISCONNECTED = 0;
const uint8_t SERVER_STATUS_RECEIVED_INIT = 1; // awaiting the first non-INIT segment
const uint8_t SERVER_STATUS_CONNECTED = 2;     // the three-way handshake is complete

const uint8_t TERMINATION_STATUS_ALIVE = 0;
const uint8_t TERMINATION_STATUS_TERMINATING = 1;
const uint8_t TERMINATION_STATUS_TERMINATED = 2;

/* segment structure:
 * (4 bytes) (+0) error detection
 * (4 bytes) (+4) segment type
 * (4 bytes) (+8) sequence number (except for SEGMENT_TYPE_BYE and SEGMENT_TYPE_BYE_ACK)
 * (4 bytes) (+12) payload size (only used for SEGMENT_TYPE_MESSAGE)
 * (remaining bytes) (+16) payload (only used for SEGMENT_TYPE_MESSAGE)
 */

const char* const filename_servertoclient = "/tmp/trabalho-redes-transporte-stc";
const char* const filename_clienttoserver = "/tmp/trabalho-redes-transporte-cts";

struct Connection {
    uint8_t type;
    uint8_t server_status; // only used in the server
    uint8_t termination_status;
    uint32_t sender_nseq, receiver_nseq;
    uint8_t segments[GO_BACK][SEGMENT_SIZE];
    uint32_t segments_beg, segments_end;
    uint32_t timer_counter;
    uint8_t read_buffer[BUFFER_SIZE], write_buffer[BUFFER_SIZE];
    uint32_t read_buffer_beg, read_buffer_end;
    uint32_t write_buffer_beg, write_buffer_end;
    pthread_mutex_t read_buffer_mutex, write_buffer_mutex;
    uint32_t local_address, remote_address;
};

enum {
    CONNECTION_TYPE_SERVER = 0,
    CONNECTION_TYPE_CLIENT = 1,
};
void* start_event_loop(void* conn);
int validate_segment(uint8_t* buf);
int send_segment(Connection* conn, uint8_t* buf);
int receive_segment(Connection* conn, uint8_t* buf);
void timer(Connection* conn);
void process_segment(Connection* conn, uint8_t* segment);
void print_segment_type(uint32_t type);
void free_connection(Connection* conn);

Connection* start_transport_server(const uint32_t local_address, const uint32_t remote_address) {
    puts("(transport) Starting server...");
    Connection* conn = malloc(sizeof(Connection));
    conn->type = CONNECTION_TYPE_SERVER;
    conn->termination_status = TERMINATION_STATUS_ALIVE;
    conn->server_status = SERVER_STATUS_DISCONNECTED;
    conn->segments_beg = 0;
    conn->segments_end = 0;
    conn->timer_counter = 0;
    conn->read_buffer_beg = 0;
    conn->read_buffer_end = 0;
    conn->write_buffer_beg = 0;
    conn->write_buffer_end = 0;
    conn->local_address = local_address;
    conn->remote_address = remote_address;
    pthread_mutex_init(&conn->read_buffer_mutex, 0);
    pthread_mutex_init(&conn->write_buffer_mutex, 0);
    // Start event loop in a new thread
    puts("(transport) Starting event loop...");
    pthread_t thread;
    pthread_create(&thread, 0, &start_event_loop, conn);
    // Return the connection pointer (after the handshake is complete)
    while (conn->server_status != SERVER_STATUS_CONNECTED)
        sleep_ms(10);
    return conn;
}

Connection* start_transport_client(const uint32_t local_address, const uint32_t remote_address) {
    // Establish the network-layer channel
    puts("(transport) Starting client...");
    Connection* conn = malloc(sizeof(Connection));
    conn->type = CONNECTION_TYPE_CLIENT;
    conn->termination_status = TERMINATION_STATUS_ALIVE;
    conn->segments_beg = 0;
    conn->segments_end = 0;
    conn->timer_counter = 0;
    conn->read_buffer_beg = 0;
    conn->read_buffer_end = 0;
    conn->write_buffer_beg = 0;
    conn->write_buffer_end = 0;
    conn->local_address = local_address;
    conn->remote_address = remote_address;
    // Send a special segment to initiate the transport-layer connection
    puts("(transport) Sending special handshake segment...");
    uint8_t segment[SEGMENT_SIZE];
    for (int i = 0; i < 50; ++i) {
        printf("(transport) Attempt #%d\n", i);
        const uint32_t nseq = 42; // TODO: random sequence number (a fixed number is better for the presentation)
        memset(segment, 0, SEGMENT_SIZE);
        write_uint32(segment+4, SEGMENT_TYPE_INIT);
        write_uint32(segment+8, nseq);
        send_segment(conn, segment);
        sleep_ms(1000);
        if (receive_segment(conn, segment) != 0 && validate_segment(segment) && read_uint32(segment+4) == SEGMENT_TYPE_INIT_ACK && read_uint32(segment+8) == nseq) {
            puts("(transport) Received INIT ACK");
            // Finish setting up the connection
            conn->sender_nseq = nseq+1;
            conn->receiver_nseq = nseq;
            pthread_mutex_init(&conn->read_buffer_mutex, 0);
            pthread_mutex_init(&conn->write_buffer_mutex, 0);
            // Start the event loop on a separate thread
            puts("(transport) Starting event loop...");
            pthread_t thread;
            pthread_create(&thread, 0, start_event_loop, conn);
            // Return the connection pointer
            return conn;
        }
    }
    free(conn);
    return 0;
}

void* start_event_loop(void* const conn_) {
    Connection* conn = conn_;
    uint8_t segment[SEGMENT_SIZE];
    for (;;) {
        // Receive all readily available segments
        while (receive_segment(conn, segment) != 0) {
            if (!validate_segment(segment))
                continue;
            if (read_uint32(segment+4) == SEGMENT_TYPE_BYE) {
                // After receiving a termination segment, we first reply with an ACK
                puts("(transport) Received termination segment, replying with ACK...");
                uint8_t response[SEGMENT_SIZE];
                write_uint32(response+4, SEGMENT_TYPE_BYE_ACK);
                write_uint32(response+8, 0); // the nseq doesn't really matter
                send_segment(conn, response);
                // Then spend a few miliseconds replying further termination segments (in case our initial reply was lost)
                puts("(transport) Waiting for repeated termination segments...");
                for (int i = 0; i < 20; ++i) {
                    while (receive_segment(conn, segment) != 0) {
                        if (validate_segment(segment) && read_uint32(segment+4) == SEGMENT_TYPE_BYE) {
                            puts("(transport) resending ACK for termination segment");
                            send_segment(conn, response);
                        }
                    }
                    sleep_ms(10);
                }
                // And finally exit
                puts("(transport) Waited long enough, exiting...");
                conn->termination_status = TERMINATION_STATUS_TERMINATED;
                return 0;
            } else if (read_uint32(segment+4) == SEGMENT_TYPE_BYE_ACK) {
                // After receiving an ACK for a termination segment, we immediately exit
                puts("(transport) Received ACK for termination segment, exiting...");
                conn->termination_status = TERMINATION_STATUS_TERMINATED;
                return 0;
            } else {
                process_segment(conn, segment);
            }
        }
        // Send as many segments as possible
        pthread_mutex_lock(&conn->write_buffer_mutex);
        while (conn->segments_end != conn->segments_beg+GO_BACK && conn->write_buffer_beg != conn->write_buffer_end) {
            uint8_t* segment = conn->segments[conn->segments_end%GO_BACK];
            size_t payload_size = 0;
            while (payload_size < MAX_PAYLOAD_SIZE && conn->write_buffer_beg != conn->write_buffer_end) {
                segment[16+payload_size] = conn->write_buffer[conn->write_buffer_beg%BUFFER_SIZE];
                ++payload_size;
                ++conn->write_buffer_beg;
            }
            write_uint32(segment+4, SEGMENT_TYPE_MESSAGE);
            write_uint32(segment+8, conn->sender_nseq);
            write_uint32(segment+12, payload_size);
            send_segment(conn, segment);
            ++conn->segments_end;
            ++conn->sender_nseq;
        }
        pthread_mutex_unlock(&conn->write_buffer_mutex);
        // If a termination command has been issued and all previous segments have been sent and acknowledged, we may send a termination segment
        if (conn->termination_status == TERMINATION_STATUS_TERMINATING && conn->segments_beg == conn->segments_end) {
            puts("(transport) Sending termination segment");
            uint8_t segment[SEGMENT_SIZE];
            write_uint32(segment+4, SEGMENT_TYPE_BYE);
            write_uint32(segment+8, 0); // the nseq doesn't really matter
            send_segment(conn, segment);
        }
        // Call the timer (every 10ms or so)
        timer(conn);
        // Wait for new segments to arrive
        sleep_ms(10);
    }
    // Connection closed successfully
    return 0;
}

int validate_segment(uint8_t* const segment) {
    // Simplified error checking: the xor of all (32-bit) words must be zero
    uint8_t x[] = {0, 0, 0, 0};
    for (int s = 0; s < 4; ++s)
        for (int i = s; i < SEGMENT_SIZE; i += 4)
            x[s] ^= segment[i];
    return x[0] == 0 && x[1] == 0 && x[2] == 0 && x[3] == 0;
}

int send_segment(Connection* const conn, uint8_t* const segment) {
    printf("(transport) Sending segment: nseq=%u, type=", read_uint32(segment+8));
    print_segment_type(read_uint32(segment+4));
    puts("");
    // Simplified error checking: the xor of all (32-bit) words must be zero
    segment[0] = 0;
    segment[1] = 0;
    segment[2] = 0;
    segment[3] = 0;
    for (int s = 0; s < 4; ++s)
        for (int i = 4+s; i < SEGMENT_SIZE; i += 4)
            segment[s] ^= segment[i];
    // Send segment
    for (;;) {
        if (network_send(conn->local_address, conn->remote_address, segment)) {
            break;
        }
        sleep_ms(5);
    }
    return 0;
}

int receive_segment(Connection* const conn, uint8_t* const buf) {
    if (network_receive(conn->local_address, conn->remote_address, buf)) {
        return SEGMENT_SIZE;
    } else {
        return 0;
    }
}

void timer(Connection* const conn) {
    ++conn->timer_counter;
    if (conn->timer_counter == 100) { // timeout of 1s
        // Resend segments
        if (conn->segments_beg != conn->segments_end) {
            puts("(transport) Timeout, resending all segments in the window...");
            for (uint32_t i = conn->segments_beg; i != conn->segments_end; ++i) {
                send_segment(conn, conn->segments[i%GO_BACK]);
            }
        }
        // Restart counter
        conn->timer_counter = 0;
    }
}

void process_segment(Connection* const conn, uint8_t* const segment) {
    printf("(transport) Receiving segment: nseq=%u, type=", read_uint32(segment+8));
    print_segment_type(read_uint32(segment+4));
    puts("");
    // Handle handshakes
    if (read_uint32(segment+4) == SEGMENT_TYPE_INIT) {
        if (conn->type == CONNECTION_TYPE_SERVER) {
            if (conn->server_status == SERVER_STATUS_DISCONNECTED) {
                puts("(transport) Received first INIT");
                // After receiving the first handshake, we need to setup the connection and reply
                conn->server_status = SERVER_STATUS_RECEIVED_INIT;
                conn->sender_nseq = read_uint32(segment+8);
                conn->receiver_nseq = conn->sender_nseq + 1;
                uint8_t response[SEGMENT_SIZE];
                write_uint32(response+4, SEGMENT_TYPE_INIT_ACK);
                write_uint32(response+8, conn->sender_nseq);
                send_segment(conn, response);
            } else if (conn->server_status == SERVER_STATUS_RECEIVED_INIT) {
                if (read_uint32(segment+8) == conn->receiver_nseq) {

                    puts("(transport) Received repeated INIT");
                    // After receiving duplicates of the initial handshake (presumably because our ACK was lost), we also need to reply
                    // We may assume that "sender_nseq" has not been changed since we use three-way handshaking
                    uint8_t response[SEGMENT_SIZE];
                    write_uint32(response+4, SEGMENT_TYPE_INIT_ACK);
                    write_uint32(response+8, conn->sender_nseq);
                    send_segment(conn, response);
                } else {
                    // Presumably someone is trying to start another connection while this one is still active
                    puts("(transport) Received INIT with an invalid sequence number");
                }
            }
        }
        return;
    }
    // Handle message ACKs
    if (read_uint32(segment+4) == SEGMENT_TYPE_MESSAGE_ACK) {
        const uint32_t nseq = read_uint32(segment+8);
        // Just ignore the ACK if currently there are no segments in te window
        if (conn->segments_beg == conn->segments_end) {
            printf("(transport) Received message ACK with nseq=%u (ignoring due to empty window)\n", nseq);
            return;
        }
        // Also ignore duplicate ACKs (the assumption that packets are never reordered is important here)
        if (read_uint32(conn->segments[conn->segments_beg%GO_BACK]) == nseq+1) {
            printf("(transport) Received duplicate message ACK with nseq=%u\n", nseq);
            return;
        }
        // Otherwise, repeatedly advance the window
        printf("(transport) Received cumulative message ACK with nseq=%u\n", nseq);
        conn->timer_counter = 0;
        while (conn->segments_beg != conn->segments_end) {
            printf("(transport) Advancing sender window by one unit\n");
            if (read_uint32(conn->segments[(conn->segments_beg++)%GO_BACK]+8) == nseq) {
                break;
            }
        }
        return;
    }
    // Handle common messages
    if (read_uint32(segment+4) == SEGMENT_TYPE_MESSAGE) {
        const uint32_t nseq = read_uint32(segment+8);
        // If this is the first message, mark the three-way handhsake as complete and call "on_init"
        if (conn->type == CONNECTION_TYPE_SERVER && conn->server_status == SERVER_STATUS_RECEIVED_INIT) {
            conn->server_status = SERVER_STATUS_CONNECTED;
        }
        // If the message is new (and sequential), append it to the read buffer
        if (nseq == conn->receiver_nseq) {
            const uint32_t payload_size = read_uint32(segment+12);
            // Attempt to write the (entire) message to the read buffer
            pthread_mutex_lock(&conn->read_buffer_mutex);
            uint32_t pos = conn->read_buffer_end;
            size_t bwritten = 0;
            for (;;) {
                if (pos == conn->read_buffer_beg+BUFFER_SIZE) {
                    // there is not enough space in the buffer
                    pthread_mutex_unlock(&conn->read_buffer_mutex);
                    return;
                }
                conn->read_buffer[pos%BUFFER_SIZE] = segment[16+bwritten];
                ++pos;
                if (++bwritten == payload_size) {
                    break;
                }
            }
            conn->read_buffer_end += bwritten;
            pthread_mutex_unlock(&conn->read_buffer_mutex);
            // If we succeeded, we should increase the sequence number
            ++conn->receiver_nseq;
            // And reply with an ACK
            uint8_t response[SEGMENT_SIZE];
            write_uint32(response+4, SEGMENT_TYPE_MESSAGE_ACK);
            write_uint32(response+8, nseq);
            send_segment(conn, response);
        }
        // Return
        return;
    }
}

void print_segment_type(const uint32_t type) {
    if (type == SEGMENT_TYPE_INIT) {
        printf("INIT");
    } else if (type == SEGMENT_TYPE_INIT_ACK) {
        printf("INIT_ACK");
    } else if (type == SEGMENT_TYPE_MESSAGE) {
        printf("MESSAGE");
    } else if (type == SEGMENT_TYPE_MESSAGE_ACK) {
        printf("MESSAGE_ACK");
    } else if (type == SEGMENT_TYPE_BYE) {
        printf("MESSAGE_BYE");
    } else if (type == SEGMENT_TYPE_BYE_ACK) {
        printf("MESSAGE_BYE_ACK");
    } else {
        printf("???");
    }
}

ssize_t send_message(Connection* const conn, const uint8_t* const buf, const size_t bytes) {
    if (conn->termination_status != TERMINATION_STATUS_ALIVE)
        return -1;
    // Write as much as possible of the message to the write buffer
    pthread_mutex_lock(&conn->write_buffer_mutex);
    size_t bwritten = 0;
    while (conn->write_buffer_end != conn->write_buffer_beg+BUFFER_SIZE && bwritten < bytes) {
        conn->write_buffer[conn->write_buffer_end%BUFFER_SIZE] = buf[bwritten];
        ++conn->write_buffer_end;
        ++bwritten;
    }
    pthread_mutex_unlock(&conn->write_buffer_mutex);
    if (bwritten != 0) {
        printf("(transport) Adding message to the write buffer: size=%u\n", (unsigned) bwritten);
    }
    return bwritten;
}

ssize_t receive_message(Connection* const conn, uint8_t* const buf, const size_t bytes) {
    // Read as much as possible from the read buffer
    pthread_mutex_lock(&conn->read_buffer_mutex);
    size_t bread = 0;
    while (conn->read_buffer_beg != conn->read_buffer_end && bread < bytes) {
        buf[bread] = conn->read_buffer[conn->read_buffer_beg%BUFFER_SIZE];
        ++conn->read_buffer_beg;
        ++bread;
    }
    pthread_mutex_unlock(&conn->read_buffer_mutex);
    if (bread != 0) {
        printf("(transport) Taking message from the read buffer: size=%u\n", (unsigned) bread);
    }
    return bread;
}

void send_message_blocking(Connection* const conn, const uint8_t* const buf, const size_t bytes) {
    size_t pos = 0;
    while (pos < bytes) {
        const ssize_t bwritten = send_message(conn, buf+pos, bytes-pos);
        if (bwritten < 0) {
            puts("(transport) ERROR: send_message failed!");
            exit(1);
        }
        pos += bwritten;
        if (pos == bytes)
            break;
        sleep_ms(10);
    }
}

void receive_message_blocking(Connection* const conn, uint8_t* const buf, const size_t bytes) {
    size_t pos = 0;
    while (pos < bytes) {
        const ssize_t bread = receive_message(conn, buf+pos, bytes-pos);
        if (bread < 0) {
            puts("(transport) ERROR: receive_message failed!");
            exit(1);
        }
        pos += bread;
        if (pos == bytes)
            break;
        sleep_ms(10);
    }
}

void terminate_connection(Connection* const conn) {
    puts("(transport) Preparing connection for termination...");
    conn->termination_status = TERMINATION_STATUS_TERMINATING;
    wait_for_termination(conn);
}

void wait_for_termination(Connection* const conn) {
    while (conn->termination_status != TERMINATION_STATUS_TERMINATED)
        sleep_ms(10);
    free_connection(conn);
}

void free_connection(Connection* const conn) {
    pthread_mutex_destroy(&conn->read_buffer_mutex);
    pthread_mutex_destroy(&conn->write_buffer_mutex);
    free(conn);
}
