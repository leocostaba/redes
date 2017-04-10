#include <stdint.h>
#include <stdbool.h>

// Note: FRAME_MULTIPLIER must be a multiple of FRAME_SINE_FREQUENCY
// Note: FRAME_MULTIPLIER should be even
#define SAMPLE_RATE   (44100)
#define DATAGRAM_SIZE  (100)
#define FRAME_SYNCHRONIZATION_BYTES (8)
#define FRAME_SIZE (DATAGRAM_SIZE+FRAME_SYNCHRONIZATION_BYTES)
#define FRAME_SIZE_BITS (FRAME_SIZE*8)
#define FRAME_SYNCHRONIZATION_BITS (FRAME_SYNCHRONIZATION_BYTES*8)
#define FRAME_MULTIPLIER (80)
#define FRAME_REAL_SIZE_BITS (FRAME_SIZE_BITS*FRAME_MULTIPLIER)
#define FRAME_REAL_SYNCHRONIZATION_BITS (FRAME_SYNCHRONIZATION_BITS*10)
#define FRAME_SINE_FREQUENCY (8)

// Synchronization preamble
extern uint8_t link_synchronization_preamble[];

// Link-layer API
void link_start();
bool link_send(const uint8_t* datagram);
bool link_receive(uint8_t* datagram);
