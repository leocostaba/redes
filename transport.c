#include "transport.h"
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
//TODO: make it possible to end the connection

#define GO_BACK 32 // should be a power of two to correctly handle unsigned overflow in the ring buffer
#define HEADER_SIZE 16
#define MAX_PAYLOAD_SIZE 1500
#define SEGMENT_SIZE (HEADER_SIZE+MAX_PAYLOAD_SIZE)

const uint32_t SEGMENT_TYPE_INIT = 0x11213141;
const uint32_t SEGMENT_TYPE_INIT_ACK = 0x12223242;
const uint32_t SEGMENT_TYPE_MESSAGE = 0x13233343;
const uint32_t SEGMENT_TYPE_MESSAGE_ACK = 0x14243444;

const uint8_t SERVER_STATUS_DISCONNECTED = 0;
const uint8_t SERVER_STATUS_RECEIVED_INIT = 1; // awaiting the first non-INIT segment
const uint8_t SERVER_STATUS_CONNECTED = 2;     // the three-way handshake is complete

/* segment structure:
 * (4 bytes) (+0) error detection
 * (4 bytes) (+4) segment type
 * (4 bytes) (+8) sequence number
 * (4 bytes) (+12) payload size (only used for SEGMENT_TYPE_MESSAGE)
 * (remaining bytes) (+16) payload (only used for SEGMENT_TYPE_MESSAGE)
 */

const char* const filename_servertoclient = "/tmp/trabalho-redes-transporte-stc";
const char* const filename_clienttoserver = "/tmp/trabalho-redes-transporte-cts";

struct Connection {
    uint8_t type;
    int readpipe, writepipe;
    on_init_t on_init;
    on_receive_t on_receive;
    uint8_t server_status; // only used in the server
    uint32_t sender_nseq, receiver_nseq;
    uint8_t segments[GO_BACK][SEGMENT_SIZE];
    uint32_t segments_beg, segments_end;
    uint32_t timer_counter;
};

enum {
    CONNECTION_TYPE_SERVER = 0,
    CONNECTION_TYPE_CLIENT = 1,
};

int start_event_loop(Connection* conn);
int validate_segment(uint8_t* buf);
int send_segment(Connection* conn, uint8_t* buf);
int receive_segment(Connection* conn, uint8_t* buf);
void timer(Connection* conn);
void process_segment(Connection* conn, uint8_t* segment);
void print_segment_type(uint32_t type);

int start_server(const on_init_t on_init, const on_receive_t on_receive) {
    // Establish the network-layer channel
    if (access(filename_clienttoserver, F_OK) == -1) {
        puts("(transport) Pipe \"client-to-server\" does not exist, creating it...");
        if (mkfifo(filename_clienttoserver, 0666) != 0) {
            puts("(transport) ERROR: Unable to create pipe \"client-to-server\"");
            return -1;
        }
    }
    if (access(filename_servertoclient, F_OK) == -1) {
        puts("(transport) Pipe \"server-to-client\" does not exist, creating it...");
        if (mkfifo(filename_servertoclient, 0666) != 0) {
            puts("(transport) ERROR: Unable to create pipe \"server-to-client\"");
            return -1;
        }
    }
    puts("(transport) Starting server...");
    Connection conn;
    conn.type = CONNECTION_TYPE_SERVER;
    conn.readpipe = open(filename_clienttoserver, O_RDONLY | O_NONBLOCK);
    if (conn.readpipe == -1)
        return -1;
    puts("(transport) Opening write pipe...");
    while ((conn.writepipe = open(filename_servertoclient, O_WRONLY | O_NONBLOCK)) == -1) {
        sleep_ms(10);
    }
    conn.on_init = on_init;
    conn.on_receive = on_receive;
    conn.server_status = SERVER_STATUS_DISCONNECTED;
    conn.segments_beg = 0;
    conn.segments_end = 0;
    conn.timer_counter = 0;
    // Start event loop
    puts("(transport) Starting event loop...");
    return start_event_loop(&conn);
}

int start_client(const on_init_t on_init, const on_receive_t on_receive) {
    // Establish the network-layer channel
    puts("(transport) Starting client...");
    Connection conn;
    conn.type = CONNECTION_TYPE_CLIENT;
    conn.writepipe = open(filename_clienttoserver, O_WRONLY | O_NONBLOCK);
    if (conn.writepipe == -1)
        return -1;
    conn.readpipe = open(filename_servertoclient, O_RDONLY | O_NONBLOCK);
    if (conn.readpipe == -1) {
        close(conn.writepipe);
        return -1;
    }
    conn.on_init = on_init;
    conn.on_receive = on_receive;
    conn.segments_beg = 0;
    conn.segments_end = 0;
    conn.timer_counter = 0;
    // Send a special segment to initiate the transport-layer connection
    puts("(transport) Sending special handshake segment...");
    uint8_t segment[SEGMENT_SIZE];
    for (int i = 0; i < 50; ++i) {
        printf("(transport) \tAttempt #%d\n", i);
        const uint32_t nseq = 42; // TODO: random sequence number
        memset(segment, 0, SEGMENT_SIZE);
        write_uint32(segment+4, SEGMENT_TYPE_INIT);
        write_uint32(segment+8, nseq);
        send_segment(&conn, segment);
        sleep_ms(100);
        if (receive_segment(&conn, segment) != 0 && validate_segment(segment) && read_uint32(segment+8) == nseq) {
            // Success: call "on_init" and start event loop
            conn.sender_nseq = nseq+1;
            conn.receiver_nseq = nseq;
            conn.on_init(&conn);
            puts("(transport) Starting event loop...");
            return start_event_loop(&conn);
        }
    }
    return -1;
}

int start_event_loop(Connection* const conn) {
    uint8_t segment[SEGMENT_SIZE];
    for (;;) {
        // Receive all readily available segments
        while (receive_segment(conn, segment) != 0) {
            if (validate_segment(segment)) {
                process_segment(conn, segment);
            }
        }
        // Call the timer (every 10ms or so)
        timer(conn);
        // Wait 10ms for new segments to arrive
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
    printf("(transport) Sending segment: %u (", read_uint32(segment+8));
    print_segment_type(read_uint32(segment+4));
    puts(")");
    // Simplified error checking: the xor of all (32-bit) words must be zero
    segment[0] = 0;
    segment[1] = 0;
    segment[2] = 0;
    segment[3] = 0;
    for (int s = 0; s < 4; ++s)
        for (int i = 4+s; i < SEGMENT_SIZE; i += 4)
            segment[s] ^= segment[i];
    // Send segment
    const ssize_t bwritten = write(conn->writepipe, segment, SEGMENT_SIZE);
    if (bwritten == -1)
        exit(1);
    if ((size_t) bwritten < SEGMENT_SIZE)
        exit(2);
    return 0;
}

int receive_segment(Connection* const conn, uint8_t* const buf) {
    const ssize_t bread = read(conn->readpipe, buf, SEGMENT_SIZE);
    if (bread == 0)
        return 0;
    if (bread == -1) {
        if (errno == EAGAIN) {
            return 0;
        } else {
            printf("(transport) ERROR: receive_segment: bread = -1, errno = %d\n", errno);
            exit(1);
        }
    }
    if ((size_t) bread < SEGMENT_SIZE) {
        puts("(transport) ERROR: receive_segment: bread < SEGMENT_SIZE");
        exit(2);
    }
    return bread;
}

void timer(Connection* const conn) {
    ++conn->timer_counter;
    if (conn->timer_counter == 100) { // timeout of 1s
        // Resend segments
        for (uint32_t i = conn->segments_beg; i != conn->segments_end; ++i) {
            send_segment(conn, conn->segments[i%GO_BACK]);
        }
        // Restart counter
        conn->timer_counter = 0;
    }
}

void process_segment(Connection* const conn, uint8_t* const segment) {
    printf("(transport) Receiving segment: %u (", read_uint32(segment+8));
    print_segment_type(read_uint32(segment+4));
    puts(")");
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
    // Handle ACKs
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
        while (conn->segments_beg != conn->segments_end) {
            printf("(transport)\tAdvancing sender window by one unit\n");
            if (read_uint32(conn->segments[(conn->segments_beg++)%GO_BACK]) == nseq) {
                break;
            }
        }
        return;
    }
    // Handle common messages
    if (read_uint32(segment+4) == SEGMENT_TYPE_MESSAGE) {
        const uint32_t nseq = read_uint32(segment+8);
        // If this is the first message, mark the three-way handhsake as complete and call "on_init"
        if (conn->server_status == SERVER_STATUS_RECEIVED_INIT) {
            conn->server_status = SERVER_STATUS_CONNECTED;
            conn->on_init(conn);
        }
        // If the message is new (and sequential), forward it to the upper layer
        if (nseq == conn->receiver_nseq) {
            const uint32_t payload_size = read_uint32(segment+12);
            conn->on_receive(conn, segment+16, payload_size);
            ++conn->receiver_nseq;
        }
        // Send ACK
        uint8_t response[SEGMENT_SIZE];
        write_uint32(response+4, SEGMENT_TYPE_MESSAGE_ACK);
        write_uint32(response+8, nseq);
        send_segment(conn, response);
        // Return
        return;
    }
}

ssize_t send_message(Connection* const conn, const uint8_t* const buf, const size_t bytes) {
    const size_t required_segments = (bytes / MAX_PAYLOAD_SIZE) + (bytes % MAX_PAYLOAD_SIZE != 0);
    ssize_t bwritten = 0;
    for (uint32_t s = 0; s < required_segments && conn->segments_beg+GO_BACK != conn->segments_end; ++s) {
        uint8_t* const segment = conn->segments[conn->segments_end%GO_BACK];
        const uint8_t* const payload_begin = buf + s*MAX_PAYLOAD_SIZE;
        const size_t payload_size = s+1 == required_segments ? bytes % MAX_PAYLOAD_SIZE : MAX_PAYLOAD_SIZE;
        bwritten += payload_size;
        write_uint32(segment+4, SEGMENT_TYPE_MESSAGE);
        write_uint32(segment+8, conn->sender_nseq);
        write_uint32(segment+12, payload_size);
        memcpy(segment+16, payload_begin, payload_size);
        send_segment(conn, segment);
        ++conn->segments_end;
        ++conn->sender_nseq;
    }
    return bwritten;
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
    } else {
        printf("???");
    }
}
