#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include <pthread.h>
#include "link.h"

#define MAX_DATAGRAMS (32) // should be a power of two to correctly handle unsigned overflow in the ring buffer (not that it will ever happen...)

static pthread_mutex_t datagrams_mutex;
static uint8_t datagrams[MAX_DATAGRAMS][DATAGRAM_SIZE];
static uint32_t datagrams_beg = 0, datagrams_end = 0;

bool link_send(const uint8_t* datagram) {
    pthread_mutex_lock(&datagrams_mutex);
    if (datagrams_end == datagrams_beg+MAX_DATAGRAMS) {
        pthread_mutex_unlock(&datagrams_mutex);
        return false;
    }
    memcpy(datagrams[datagrams_end%MAX_DATAGRAMS], datagram, DATAGRAM_SIZE);
    ++datagrams_end;
    pthread_mutex_unlock(&datagrams_mutex);
    return true;
}

int iter = 0;
static int patestCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Prevent unused variable warnings
    (void) timeInfo;
    (void) framesPerBuffer;
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;
    // Build frame
    uint8_t frame[FRAME_SIZE];
    {
        // Write synchronization bytes
        for (int i = 0; i < FRAME_SYNCHRONIZATION_BYTES; ++i)
            frame[i] = link_synchronization_preamble[i];
        // Write datagram content
#if DOUGLAS_ADAMS==1
        if ((++iter)&1) {
            memcpy(frame+FRAME_SYNCHRONIZATION_BYTES, "The Guide is definitive. Reality is frequently innacurate. In the beginning the Universe was created. This has made a lot of people very angry and been widely regarded as a bad move. Some more text here just in case someone decides to increase the datagram size even more. Some more text here just in case someone decides to increase the datagram size even more.", DATAGRAM_SIZE);
        } else {
            memcpy(frame+FRAME_SYNCHRONIZATION_BYTES, "The following proposition is occasionally useful. I repeat, the following proposition is occasionally useful. Some more text here just in case someone decides to increase the datagram size even more. Some more text here just in case someone decides to increase the datagram size even more.", DATAGRAM_SIZE);
        }
#else
        pthread_mutex_lock(&datagrams_mutex);
        if (datagrams_beg == datagrams_end) {
            for (int i = 0; i < DATAGRAM_SIZE; ++i)
                frame[FRAME_SYNCHRONIZATION_BYTES+i] = rand()%2;
        } else {
            printf("(link transmitter) sending a datagram\n");
            memcpy(frame+FRAME_SYNCHRONIZATION_BYTES, datagrams[datagrams_beg%MAX_DATAGRAMS], DATAGRAM_SIZE);
            ++datagrams_beg;
        }
        pthread_mutex_unlock(&datagrams_mutex);
#endif
    }
    // Convert frame into audio
    float* out = (float*) outputBuffer;
    for (int i = 0; i < FRAME_SIZE; ++i) {
        //printf("frame[%d] = %d\n", i, frame[i]);
        for (int j = 7; j >= 0; --j) {
            const bool bit = (frame[i] >> j) & 1;
            //const bool bit = 1;
            //const bool bit = 0;
            //const bool bit = (j%4)==0;
            //const bool bit = j&1;
            //const bool bit = (j%4)>=2;
            //const bool bit = j>=4;
            //printf("bit=%d\n", (int) bit);
            for (int k = 0; k < FRAME_MULTIPLIER; ++k) {
                float x = sin((double)2*M_PI*k/FRAME_MULTIPLIER*FRAME_SINE_FREQUENCY);
                *out++ = bit ? x : 0;
            }
        }
    }
    // Return value
    return paContinue;
}

void* start_link_transmitter(void* p) {
    PaError err = paNoError;
    (void) p;
    // Initialize mutex
    pthread_mutex_init(&datagrams_mutex, 0);
    // Setup output parameters
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    // Start transmitting
    PaStream *stream;
    err = Pa_OpenStream(&stream, 0, &outputParameters, SAMPLE_RATE, FRAME_REAL_SIZE_BITS, paClipOff, patestCallback, 0);
    if (err != paNoError)
        goto error;
    err = Pa_StartStream(stream);
    if (err != paNoError)
        goto error;
    for (;;) {
        Pa_Sleep(1000);
    }
    err = Pa_StopStream(stream);
    if (err != paNoError) goto error;
    err = Pa_CloseStream( stream );
    if (err != paNoError) goto error;
    // Exit
    exit(err);
error:
    fprintf(stderr, "An error occured while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    exit(err);
}

#if DOUGLAS_ADAMS==1
int main()
{
    Pa_Initialize();
    start_link_transmitter(0);
}
#endif
