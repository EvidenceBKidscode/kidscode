#!/bin/sh
find . -name "*.lua" -exec md5sum '{}' \; \
| awk 'BEGIN{FS="  ./";OFS=""}{print "update maps set mods_hash_md5 = \"",$1,"\" where url_download = \"http://assets.kidscode.com/master/",$2,"\";"}'
