#!/bin/sh
set -x

cat *.lua

mkdir -p encrypted
rm encrypted/*.lua
cp *.lua encrypted
./crypt -e encrypted/*.lua

mkdir -p decrypted
rm decrypted/*.lua
cp encrypted/*.lua decrypted
./crypt -d decrypted/*.lua
cat decrypted/*.lua

cd encrypted
find . -name "*.lua" > CryptList.txt
../../crypt -d
cat *.lua

cd ../decrypted
find . -name "*.lua" > CryptList.txt
../../crypt -e
../../crypt -d
cat *.lua
