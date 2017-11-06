type *.lua

md encrypted
del encrypted\*.lua
copy *.lua encrypted

pause

..\Debug\crypt -e encrypted\init.lua encrypted\inventory.lua encrypted\test.lua

pause

md decrypted
del decrypted\*.lua
copy encrypted\*.lua decrypted

pause

..\Debug\crypt -d decrypted\init.lua decrypted\inventory.lua decrypted\test.lua

pause

type decrypted\*.lua

pause

cd encrypted
dir /b "*.lua" > CryptList.txt

pause

..\..\Debug\crypt -d

pause

type *.lua

pause

cd ..\decrypted
dir /b "*.lua" > CryptList.txt

pause

..\..\Debug\crypt -e

pause

..\..\Debug\crypt -d

pause

type *.lua
