#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

bool network_send(const uint32_t local_address, const uint32_t remote_address, const uint8_t* segment);
bool network_receive(const uint32_t local_address, const uint32_t remote_address, uint8_t* segment);
