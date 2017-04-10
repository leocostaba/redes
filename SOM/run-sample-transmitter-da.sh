#!/bin/bash
set -e

g++ link.cpp link_transmitter.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o /tmp/sample-transmitter-da -DDOUGLAS_ADAMS=1
/tmp/sample-transmitter-da
