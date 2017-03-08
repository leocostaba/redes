#include <stdint.h>
#include <unistd.h>

void write_uint32(uint8_t* buf, uint32_t value);
uint32_t read_uint32(const uint8_t* buf);
void sleep_ms(unsigned int ms);
