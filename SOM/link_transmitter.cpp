#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include "link.hpp"

#define MAX_DATAGRAMS (32) // should be a power of two to correctly handle unsigned overflow in the ring buffer (not that it will ever happen...)

uint8_t datagrams[MAX_DATAGRAMS][DATAGRAM_SIZE];
uint32_t datagrams_beg = 0, datagrams_end = 0;

bool send_datagram(const uint8_t* datagram) {
    if (datagrams_end == datagrams_beg+MAX_DATAGRAMS) {
        return false;
    }
    memcpy(datagrams[datagrams_end%MAX_DATAGRAMS], datagram, DATAGRAM_SIZE);
    ++datagrams_end;
}

bool last_bit = 0;
static int patestCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Prevent unused variable warnings
    (void) timeInfo;
    (void) statusFlags;
    (void) inputBuffer;
    (void) userData;
    // Build frame
    uint8_t frame[FRAME_SIZE];
    {
        // Write synchronization bytes
        for (int i = 0; i < FRAME_SYNCHRONIZATION_BYTES; ++i)
            frame[i] = synchronization[i];
        // Write datagram content
        if (datagrams_beg == datagrams_end) {
            for (int i = 0; i < DATAGRAM_SIZE; ++i)
                frame[FRAME_SYNCHRONIZATION_BYTES+i] = 0;
        } else {
            memcpy(frame+FRAME_SYNCHRONIZATION_BYTES, datagrams[datagrams_beg%MAX_DATAGRAMS], DATAGRAM_SIZE);
            //for (int i = 0; i < DATAGRAM_SIZE; ++i) {
                //frame[FRAME_SYNCHRONIZATION_BYTES+i] = rand();
            //}
            //++datagrams_beg; //temporarily commented
        }
        // Write checksum (TODO)
    }
    // Convert frame into audio
    puts("aqui");
    float* out = (float*) outputBuffer;
    for (int i = 0; i < FRAME_SIZE; ++i) {
        printf("frame[%d] = %d\n", i, frame[i]);
        for (int j = 7; j >= 0; --j) {
            const bool bit = (frame[i] >> j) & 1;
            //const bool bit = 1;
            //const bool bit = 0;
            //const bool bit = (j%4)==0;
            //const bool bit = j&1;
            //const bool bit = (j%4)>=2;
            //const bool bit = j>=4;
            printf("bit=%d\n", (int) bit);
            for (int k = 0; k < FRAME_MULTIPLIER; ++k) {
                float x = sin((double)2*M_PI*k/FRAME_MULTIPLIER*FRAME_SINE_FREQUENCY);
                *out++ = bit ? x : 0;
            }
        }
    }
    // Return value
    return paContinue;
}

/*
 * This routine is called by portaudio when playback is done.
 */
static void StreamFinished( void* userData )
{
}

/*******************************************************************/
int main()
{
    // Send some random datagram
    uint8_t datagram[DATAGRAM_SIZE];
    for (int i = 0; i < DATAGRAM_SIZE; ++i)
        datagram[i] = rand();
    //for (int i = 0; i < DATAGRAM_SIZE; ++i)
        //datagram[i] = i&1;
    memset(datagram, 0, sizeof datagram);
    for (int i = 0; i < DATAGRAM_SIZE; ++i)
        for (int j = 0; j < 8; ++j)
            datagram[i] |= (j&1) << j;
    memset(datagram, 255, sizeof datagram);
    send_datagram(datagram);

    PaStreamParameters outputParameters;
    PaStream *stream;
    PaError err;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &stream,
              0, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAME_REAL_SIZE_BITS, // frames per buffer
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              0);
    if( err != paNoError ) goto error;

    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

#define NUM_SECONDS 5
    printf("Play for %d seconds.\n", NUM_SECONDS );
    for (;;) {
        Pa_Sleep(1000);
    }

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    Pa_Terminate();
    printf("Test finished.\n");

    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
