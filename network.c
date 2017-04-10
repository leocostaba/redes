#include "network.h"
#include "link.h"
#include "util.h"
#include <string.h>

bool network_send(const uint32_t local_address, const uint32_t remote_address, const uint8_t* segment) {
    uint8_t datagram[DATAGRAM_SIZE];
    write_uint32(datagram, local_address);
    write_uint32(datagram+4, remote_address);
    memcpy(datagram+8, segment, DATAGRAM_SIZE-8);
    return link_send(datagram);
}

bool network_receive(const uint32_t local_address, const uint32_t remote_address, uint8_t* segment) {
    uint8_t datagram[DATAGRAM_SIZE];
    while (link_receive(datagram)) {
        const uint32_t local = read_uint32(datagram);
        const uint32_t remote = read_uint32(datagram+4);
        if (local == local_address && remote == remote_address) {
            memcpy(segment, datagram+8, DATAGRAM_SIZE-8);
            return true;
        }
    }
    return false;
}
