#!/bin/bash
set -e

g++ link_receiver.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-receiver-simple -DDONT_LOOK_FOR_PATTERN=1
/tmp/square-receiver-simple
