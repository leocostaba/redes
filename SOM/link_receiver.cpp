#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "portaudio.h"
#include "link.hpp"

float leftover_buffer[FRAME_REAL_SIZE_BITS];
int leftover_buffer_size = 0;

int teste = 0;

static int recordCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Prevent unused variable warnings
    (void) outputBuffer;
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;
    // Compute the amplitude
    const float *buf = (const float*) inputBuffer;
    float amplitude = 0;
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        if (fabs(buf[i]) > amplitude)
            amplitude = fabs(buf[i]);
    const float avg2_cutoff = amplitude / sqrt(2);
    // Print the buffer
    #if 0
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        printf("%f\n", buf[i]*1000);
    exit(0);
    #endif
    // Attempt to normalize the wave
    #if 0
    int sz = FRAME_MULTIPLIER / 2;
    float buf2[FRAME_REAL_SIZE_BITS];
    for (int i = 0; i+sz <= FRAME_REAL_SIZE_BITS; ++i) {
        float sum = 0;
        for (int j = 0; j < sz; ++j)
            sum += buf[i+j]*buf[i+j];
        buf2[i] = sqrt(sum) / sz;
    }
    for (int i = 0; i+sz <= FRAME_REAL_SIZE_BITS; ++i) {
        printf("%f\n", buf2[i]);
    }
    exit(0);
    #endif
    // Build superbuf
    float superbuf[2*FRAME_REAL_SIZE_BITS];
    float *fptr = superbuf;
    for (int i = 0; i < leftover_buffer_size; ++i)
        *fptr++ = leftover_buffer[i];
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        *fptr++ = buf[i];
    const int superbuf_size = fptr - superbuf;
    // Compute the best alignment
    int best_alignment = -1, best_alignment_value = -1;
    for (int alignment = 0; alignment < FRAME_MULTIPLIER; ++alignment) {
        int value = 0;
        fptr = superbuf+alignment;
        for (int i = 0; i < FRAME_SIZE_BITS-5; ++i) {
            int ones = 0;
            for (int j = 0; j < 8; ++j) {
                float avg2 = 0;
                for (int k = 0; k < FRAME_MULTIPLIER/FRAME_SINE_FREQUENCY; ++k) {
                    float x = fptr[i*FRAME_MULTIPLIER+j*FRAME_MULTIPLIER/FRAME_SINE_FREQUENCY+k];
                    avg2 += x*x;
                }
                avg2 /= FRAME_MULTIPLIER/FRAME_SINE_FREQUENCY;
                avg2 *= 2*M_PI;
                avg2 = sqrt(avg2);
                ones += avg2 >= avg2_cutoff;
            }
            if (ones < FRAME_SINE_FREQUENCY/2)
                ones = FRAME_SINE_FREQUENCY-ones;
            value += ones*ones;
        }
        if (value > best_alignment_value) {
            best_alignment = alignment;
            best_alignment_value = value;
        }
    }
    printf("alignment = %d, value = %d\n", best_alignment, best_alignment_value);
    printf("cutoff = %f\n", avg2_cutoff);
    // Build superframe
    bool superframe[FRAME_SIZE_BITS];
    bool* ptr = superframe;
    for (int i = 0; best_alignment+i*FRAME_MULTIPLIER+FRAME_MULTIPLIER-1 < superbuf_size; ++i) {
        float avg2 = 0;
        for (int j = 0; j < FRAME_MULTIPLIER; ++j) {
            float x = superbuf[best_alignment+i*FRAME_MULTIPLIER+j];
            avg2 += x*x;
        }
        avg2 /= FRAME_MULTIPLIER;
        avg2 *= 2*3.1415926535;
        avg2 = sqrt(avg2);
        *ptr++ = avg2 >= avg2_cutoff;
        //printf("val = %f\n", avg2);
    }
    const int superframe_size = ptr - superframe;
    for (int i = 0; i < superframe_size; ++i)
        printf("superframe[%d] = %d\n", i, (int) superframe[i]);
    // Search for the synchronization pattern
    int pattern_beg = -1;
    for (int i = 0; i+FRAME_SIZE_BITS <= superframe_size; ++i) {
        ptr = superframe+i;
        bool okay = true;
        for (int j = 0; j < FRAME_SYNCHRONIZATION_BYTES; ++j) {
            for (int k = 7; k >= 0; --k) {
                const bool expected = (synchronization[j] >> k) & 1;
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
        memcpy(leftover_buffer, buf, sizeof leftover_buffer);
        leftover_buffer_size = FRAME_REAL_SIZE_BITS;
    }
    // Otherwise...
#if 0
    if (++teste >= 20) {
        printf("superframe size = %d\n", superframe_size);
        printf("superframe 2 size = %d\n", superframe2_size);
        for (int i = 0; i < superframe2_size; ++i) {
            printf("buf[%d] = %d\n", i, superframe2[i]);
        }
        exit(0);
#if 0
    // Read current frame
    bool current_frame[FRAME_REAL_SIZE_BITS];
    for (int i = 1; i < FRAME_REAL_SIZE_BITS; ++i)
        current_frame[i] = buf[i] >= 0;
    // Print current frame
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i) {
        //printf("current_frame[%03d] = %f\n", i, buf[i]*1000);
        printf("%f\n", buf[i]*1000);
        //printf("current_frame[%03d] = %d\n", i, (int)current_frame[i]);
        if (i >= 5000) exit(0);
    }
    exit(0);
#endif
#if 0
    // Build superframe
    bool superframe[2*FRAME_REAL_SIZE_BITS];
    bool *ptr = superframe;
    for (int i = 0; i < leftover_bits_count; ++i)
        *ptr++ = leftover_bits[i];
    for (int i = 0; i < FRAME_REAL_SIZE_BITS; ++i)
        *ptr++ = current_frame[i];
    const int superframe_size = ptr - superframe;
    // Compute the best alignment
    int best_alignment = -1, best_alignment_value = -1;
    for (int alignment = 0; alignment < FRAME_MULTIPLIER; ++alignment) {
        int value = 0;
        ptr = superframe+alignment;
        for (int i = 0; i < FRAME_SIZE_BITS-5; ++i) {
            int ones = 0;
            for (int j = 0; j < FRAME_MULTIPLIER; ++j)
                ones += *ptr++;
            if (ones < 5)
                ones = 10-ones;
            value += ones*ones;
        }
        if (value > best_alignment_value) {
            best_alignment = alignment;
            best_alignment_value = value;
        }
    }
    printf("alignment = %d, value = %d\n", best_alignment, best_alignment_value);
    // Convert the superframe into bits
    bool superframe2[2*FRAME_SIZE_BITS];
    ptr = superframe2;
    for (int i = 0; best_alignment+i*FRAME_MULTIPLIER+FRAME_MULTIPLIER-1 < superframe_size; ++i) {
        int ones = 0;
        for (int j = 0; j < FRAME_MULTIPLIER; ++j)
            ones += superframe[best_alignment+i*FRAME_MULTIPLIER+j];
        *ptr++ = ones >= 5;
    }
    const int superframe2_size = ptr - superframe2;
#if 0
    if (++teste >= 20) {
        printf("superframe size = %d\n", superframe_size);
        printf("superframe 2 size = %d\n", superframe2_size);
        for (int i = 0; i < superframe2_size; ++i) {
            printf("buf[%d] = %d\n", i, superframe2[i]);
        }
        exit(0);
    }
#endif
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
    //printf("frame_size_bits = %d\n", FRAME_SIZE_BITS);
    //printf("superframe2 size = %d\n", superframe2_size);
    for (int i = 0; i + FRAME_SIZE_BITS <= superframe2_size; ++i) {
        ptr = superframe2+i;
        bool okay = true;
        for (int j = 0; j < FRAME_SYNCHRONIZATION_BYTES; ++j) {
            for (int k = 0; k < 8; ++k) {
                const bool expected = (synchronization[j] >> k) & 1;
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
    // Otherwise...
    } else {
    }
#endif
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
    err = Pa_OpenStream(&stream, &inputParameters, 0, SAMPLE_RATE, FRAME_REAL_SIZE_BITS, paClipOff, recordCallback, 0);
    if(err != paNoError)
        goto error;
    err = Pa_StartStream(stream);
    if(err != paNoError)
        goto error;
    while ((err = Pa_IsStreamActive(stream)) == 1) {
        Pa_Sleep(1000);
    }
    if (err < 0)
        goto error;
    err = Pa_CloseStream(stream);
    if(err != paNoError)
        goto error;
    // Exit
    return 0;
    // Handle errors
error:
    Pa_Terminate();
    if(err != paNoError) {
        fprintf(stderr, "An error occured while using the portaudio stream\n");
        fprintf(stderr, "Error number: %d\n", err);
        fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
        exit(1);
    }
}
