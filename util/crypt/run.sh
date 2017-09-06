#!/bin/sh
set -x

mkdir -p test/encrypted
cp test/*.lua test/encrypted
./crypt -e test/encrypted/*.lua

mkdir -p test/decrypted
cp test/encrypted/*.lua test/decrypted
./crypt -d test/decrypted/*.lua
