#!/bin/sh
set -x
cd ..
g++ -g -I ../../src/ crypt.cpp -o crypt
