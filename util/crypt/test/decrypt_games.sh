#!/bin/sh
set -x

cd ../../../games
ls
find . -name "*.lua" > CryptList.txt
../util/crypt/crypt -d
