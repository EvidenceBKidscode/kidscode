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
find . -iregex ".+lua" > CryptList.txt
../../crypt -d
cat *.lua

cd ../decrypted
find . -iregex ".+lua" > CryptList.txt
../../crypt -e
../../crypt -d
cat *.lua
