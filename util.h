#include <stdint.h>

void write_uint32(uint8_t* const buf, const uint32_t value) {
    // network order
    buf[0] = (value>>24);
    buf[1] = (value>>16)&0xff;
    buf[2] = (value>>8)&0xff;
    buf[3] = value&0xff;
}

uint32_t read_uint32(const uint8_t* const buf) {
    // network order
    uint32_t value = 0;
    const uint8_t b1 = buf[0];
    const uint8_t b2 = buf[1];
    const uint8_t b3 = buf[2];
    const uint8_t b4 = buf[3];
    value |= ((uint32_t)b1)<<24;
    value |= ((uint32_t)b2)<<16;
    value |= ((uint32_t)b3)<<8;
    value |= ((uint32_t)b4);
    return value;
}
