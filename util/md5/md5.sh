#!/bin/sh
find . -name "*.lua" -exec md5sum '{}' \; | awk 'BEGIN{FS="  ./";OFS=""}{print "replace into DATABASE.TABLE(Filename,Signature) values(\"",$2,"\",\"",$1,"\");"}'
