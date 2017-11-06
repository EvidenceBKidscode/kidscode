#!/bin/sh
set -x

cd ../../../games
find . -name "*.lua" > CryptList.txt
ls
../util/crypt/crypt -e
