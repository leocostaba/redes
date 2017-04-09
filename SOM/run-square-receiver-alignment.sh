#!/bin/bash
set -e

g++ link_receiver.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o square-receiver -DDISPLAY_STREAKS=0
./square-receiver
