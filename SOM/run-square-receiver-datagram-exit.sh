#!/bin/bash
set -e

g++ link_receiver.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-receiver-datagram-exit -DDISPLAY_DATAGRAM=2
/tmp/square-receiver-datagram-exit
