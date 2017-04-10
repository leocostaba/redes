#!/bin/bash
set -e

g++ link.c link_transmitter.c link_receiver.c -std=c99 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-receiver -DCOMPILE_RECEIVER_MAIN=1
/tmp/square-receiver
