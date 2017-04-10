#!/bin/bash
set -e

g++ link.cpp link_receiver.cpp -std=c11 -lrt -lm -lasound -lportaudio -pthread -o /tmp/square-receiver
/tmp/square-receiver
