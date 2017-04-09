#!/bin/bash
set -e

g++ link_transmitter.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o sample-transmitter
./sample-transmitter
