#include "link.h"
#include <pthread.h>

uint8_t link_synchronization_preamble[] = {42, 54, 127, 200, 30, 40, 50, 60, 120, 130, 140, 150, 200, 210, 220, 230};
//uint8_t link_synchronization_preamble[] = {54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54};

void* start_link_transmitter(void*);
void* start_link_receiver(void*);

void link_start() {
    pthread_t thread_transmitter;
    pthread_create(&thread_transmitter, 0, &start_link_transmitter, 0);
    pthread_t thread_receiver;
    pthread_create(&thread_receiver, 0, &start_link_receiver, 0);
}
