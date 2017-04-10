#!/bin/bash
set -e

g++ link.c link_transmitter.c link_receiver.c -std=c99 -lrt -lm -lasound -lportaudio -pthread -o /tmp/sample-transmitter-da -DDOUGLAS_ADAMS=1
/tmp/sample-transmitter-da
