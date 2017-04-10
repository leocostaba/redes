#!/bin/bash
set -e

g++ square.cpp -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-transmitter
/tmp/square-transmitter
