#!/bin/bash
set -e

g++ link.cpp link_receiver.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-receiver-datagram -DDISPLAY_DATAGRAM=1
/tmp/square-receiver-datagram
