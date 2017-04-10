#!/bin/bash
set -e

g++ square.c -std=c99 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-transmitter
/tmp/square-transmitter
