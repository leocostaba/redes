#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

#define SAMPLE_RATE   (44100)
#define DATAGRAM_SIZE  (250)
#define FRAME_SYNCHRONIZATION_BYTES (16)
#define FRAME_CHECKSUM_BYTES (4)
#define FRAME_SIZE (DATAGRAM_SIZE+FRAME_SYNCHRONIZATION_BYTES+FRAME_CHECKSUM_BYTES)
#define FRAME_SIZE_BITS (FRAME_SIZE*8)
#define MAX_DATAGRAMS (32) // should be a power of two to correctly handle unsigned overflow in the ring buffer (not that it will ever happen...)

uint8_t datagrams[MAX_DATAGRAMS][DATAGRAM_SIZE];
uint32_t datagrams_beg = 0, datagrams_end = 0;
uint8_t synchronization[] = {42, 54, 127, 200, 30, 40, 50, 60, 120, 130, 140, 150};

bool send_datagram(const uint8_t* datagram) {
    if (datagrams_end == datagrams_beg+MAX_DATAGRAMS) {
        return false;
    }
    memcpy(datagrams[datagrams_end%MAX_DATAGRAMS], datagram, DATAGRAM_SIZE);
    ++datagrams_end;
}

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
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
        for (int i = 0; i < FRAME_SYNCHRONIZATION_BYTES; ++i) {
            frame[i] = synchronization[i];
        }
        // Write datagram content
        if (datagrams_beg == datagrams_end) {
            for (int i = 0; i < DATAGRAM_SIZE; ++i) {
                frame[FRAME_SYNCHRONIZATION_BYTES+i] = 0;
            }
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
    float* out = (float*) outputBuffer;
    for (int i = 0; i < FRAME_SIZE; ++i) {
        for (int j = 0; j < 8; ++j) {
            const bool bit = frame[i] >> j;
            const int value = bit ? 1 : -1;
            *out++ = value;
            *out++ = value;
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
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &stream,
              0, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAME_SIZE_BITS, // frames per buffer
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
    Pa_Sleep( NUM_SECONDS * 1000 );

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
