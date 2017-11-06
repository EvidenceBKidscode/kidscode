cd ..\..\..\games
dir /b /s "*.lua" > CryptList.txt
dir
..\util\crypt\Debug\crypt -d
cd ..\util\crypt\test
