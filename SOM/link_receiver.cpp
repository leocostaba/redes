#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "portaudio.h"
#include "link.hpp"

//#define DISPLAY_STREAKS 1 // 1 for square streaks

bool leftover_bits[FRAME_REAL_SIZE_BITS];
int leftover_bits_count = 0;

//TODO: try all possible alignments (discarding up to ten bits in the beginning)
static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Prevent unused variable warnings
    (void) outputBuffer;
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;
    // Read current frame
    const float *buf = (const float*) inputBuffer;
    bool current_frame[FRAME_REAL_SIZE_BITS];
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        current_frame[i] = buf[i] >= 0;
    // Build superframe
    bool superframe[2*FRAME_REAL_SIZE_BITS];
    bool *ptr = superframe;
    for (int i = 0; i < leftover_bits_count; ++i)
        *ptr++ = leftover_bits[i];
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        *ptr++ = current_frame[i];
    int superframe_size = ptr - superframe;
    // Convert the superframe into bits
    bool superframe2[FRAME_REAL_SIZE_BITS];
    bool* superframe2_ptr = superframe2;
    int last = -1, repeated = 0;
    for (int i = 0; i < superframe_size; ++i) {
        if (superframe[i] == last) {
            ++repeated;
            if (repeated == FRAME_MULTIPLIER) {
                repeated = 0;
                *superframe2_ptr++ = last;
                last = -1;
            }
        } else {
            if (repeated >= (FRAME_MULTIPLIER/2)) {
                *superframe2_ptr++ = last;
            }
            last = superframe[i];
            repeated = 1;
        }
    }
    int superframe2_size = superframe2_ptr - superframe2;
    // Optionally display streaks
#if DISPLAY_STREAKS == 1
    int streak = 0;
    for (int i = 1; i < superframe2_size; ++i) {
        if (superframe2[i] == superframe2[i-1]) {
            printf("streak = %d\n", streak);
            streak = 0;
        } else {
            ++streak;
        }
    }
    printf("streak = %d\n", streak);
#endif
    // Search for the synchronization pattern
    int pattern_beg = -1;
    for (int i = 0; i + FRAME_SYNCHRONIZATION_BITS < superframe2_size; ++i) {
        ptr = superframe2+i;
        bool okay = true;
        for (int j = 0; j < FRAME_SYNCHRONIZATION_BYTES; ++j) {
            for (int k = 0; k < 8; ++k) {
                bool expected = (synchronization[j] >> k) & 1;
                okay &= *ptr++ == expected;
                if (!okay)
                    goto next_iteration;
            }
        }
        puts("show de bola");
        exit(0);
        pattern_beg = i;
        break;
next_iteration:;
    }
#if 0
    int pattern_beg = -1;
    for (int i = 0; i + FRAME_REAL_SYNCHRONIZATION_BITS < superframe_size; ++i) {
        ptr = superframe+i;
        bool okay = true;
        for (int j = 0; j < FRAME_SYNCHRONIZATION_BYTES; ++j) {
            for (int k = 0; k < 8; ++k) {
                bool expected = synchronization[j] >> k;
                for (int l = 0; l < 10; ++l)
                    okay &= *ptr++ == expected;
                if (!okay)
                    goto next_iteration;
            }
        }
        puts("show de bola");
        exit(0);
        pattern_beg = i;
        break;
next_iteration:;
    }
    // If no synchronization pattern was found, just save the current frame as leftover bits
    if (pattern_beg == -1) {
        memcpy(leftover_bits, current_frame, FRAME_REAL_SIZE_BITS);
        leftover_bits_count = FRAME_REAL_SIZE_BITS;
    } else {
        // Otherwise...
    }
#endif
    // Return value
    return paContinue;
}

int main(void)
{
    // Initialize portaudio
    PaError err = Pa_Initialize();
    if(err != paNoError)
        goto error;
    // Setup input parameters
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto error;
    }
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    // Start recording
    PaStream* stream;
    err = paNoError;
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              0,
              SAMPLE_RATE,
              FRAME_REAL_SIZE_BITS,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              0);
    if(err != paNoError)
        goto error;
    err = Pa_StartStream( stream );
    if(err != paNoError)
        goto error;
    while((err = Pa_IsStreamActive(stream)) == 1)
    {
        Pa_Sleep(1000);
    }
    if( err < 0 ) goto error;

    err = Pa_CloseStream(stream);
    if(err != paNoError) goto error;
    // Exit
    return 0;
    // Handle errors
error:
    Pa_Terminate();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        exit(1);
    }
}
