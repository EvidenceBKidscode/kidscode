#!/bin/sh
set -x

cd ../../../games
ls
find . -iregex ".+lua" > CryptList.txt
../util/crypt/crypt -d
