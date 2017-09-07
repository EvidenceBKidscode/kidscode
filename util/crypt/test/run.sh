#!/bin/sh
set -x

cat *.lua

mkdir -p encrypted
rm encrypted/*.lua
cp *.lua encrypted
./crypt -e encrypted/*.lua
cat encrypted/*.lua

mkdir -p decrypted
rm decrypted/*.lua
cp encrypted/*.lua decrypted
./crypt -d decrypted/*.lua
cat decrypted/*.lua

cd encrypted
../../crypt -d
cat *.lua

cd ../decrypted
../../crypt -e
cat *.lua

../../crypt -d
cat *.lua
