#!/bin/sh
set -x

cat test/*.lua

mkdir -p test/encrypted
rm test/encrypted/*.lua
cp test/*.lua test/encrypted
./crypt -e test/encrypted/*.lua
cat test/encrypted/*.lua

mkdir -p test/decrypted
rm test/decrypted/*.lua
cp test/encrypted/*.lua test/decrypted
./crypt -d test/decrypted/*.lua
cat test/decrypted/*.lua

cd test/encrypted
../../crypt -d
cat *.lua

cd ../decrypted
../../crypt -e
cat *.lua

../../crypt -d
cat *.lua
