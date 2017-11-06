cd ..\..\..\games
dir /b /s "*.lua" > CryptList.txt
dir
..\util\crypt\Debug\crypt -e
cd ..\util\crypt\test
