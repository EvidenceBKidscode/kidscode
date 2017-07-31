#!/bin/bash

exit_error() {
    local prefix="<3>" suffix=""
    if [[ -t 2 ]]; then
	prefix=$(tput bold; tput setaf 1)
	suffix=$(tput sgr0)
    fi
    printf "$prefix$*$suffix\n" >&2
    exit 1
}

! type cmake &>/dev/null && exit_error "Please install 'cmake' to continue"
! type unzip &>/dev/null && exit_error "Please install 'unzip' to continue"
! type zip &>/dev/null && exit_error "Please install 'zip' to continue"

set -e

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
if [ $# -ne 1 ]; then
	echo "Usage: $0 <build directory>"
	exit 1
fi
builddir=$1
mkdir -p $builddir
builddir="$( cd "$builddir" && pwd )"
packagedir=$builddir/packages
libdir=$builddir/libs

toolchain_file=$dir/toolchain_mingw64.cmake
irrlicht_version=1.8.4
ogg_version=1.3.2
vorbis_version=1.3.5
curl_version=7.54.0
freetype_version=2.8
sqlite3_version=3.19.2
luajit_version=2.1.0-beta3
zlib_version=1.2.11

mkdir -p $packagedir
mkdir -p $libdir

cd $builddir

# Get stuff
[ -e $packagedir/irrlicht-$irrlicht_version.zip ] || wget http://minetest.kitsunemimi.pw/irrlicht-$irrlicht_version-win64.zip \
	-c -O $packagedir/irrlicht-$irrlicht_version.zip
[ -e $packagedir/zlib-$zlib_version.zip ] || wget http://minetest.kitsunemimi.pw/zlib-$zlib_version-win64.zip \
	-c -O $packagedir/zlib-$zlib_version.zip
[ -e $packagedir/libogg-$ogg_version.zip ] || wget http://minetest.kitsunemimi.pw/libogg-$ogg_version-win64.zip \
	-c -O $packagedir/libogg-$ogg_version.zip
[ -e $packagedir/libvorbis-$vorbis_version.zip ] || wget http://minetest.kitsunemimi.pw/libvorbis-$vorbis_version-win64.zip \
	-c -O $packagedir/libvorbis-$vorbis_version.zip
[ -e $packagedir/curl-$curl_version.zip ] || wget http://minetest.kitsunemimi.pw/curl-$curl_version-win64.zip \
	-c -O $packagedir/curl-$curl_version.zip
[ -e $packagedir/freetype2-$freetype_version.zip ] || wget http://minetest.kitsunemimi.pw/freetype2-$freetype_version-win64.zip \
	-c -O $packagedir/freetype2-$freetype_version.zip
[ -e $packagedir/sqlite3-$sqlite3_version.zip ] || wget http://minetest.kitsunemimi.pw/sqlite3-$sqlite3_version-win64.zip \
	-c -O $packagedir/sqlite3-$sqlite3_version.zip
[ -e $packagedir/luajit-$luajit_version.zip ] || wget http://minetest.kitsunemimi.pw/luajit-$luajit_version-win64.zip \
	-c -O $packagedir/luajit-$luajit_version.zip
[ -e $packagedir/openal_stripped.zip ] || wget http://minetest.kitsunemimi.pw/openal_stripped64.zip \
	-c -O $packagedir/openal_stripped.zip


# Extract stuff
cd $libdir
[ -d irrlicht ] || unzip -o $packagedir/irrlicht-$irrlicht_version.zip -d irrlicht
[ -d zlib ] || unzip -o $packagedir/zlib-$zlib_version.zip -d zlib
[ -d libogg ] || unzip -o $packagedir/libogg-$ogg_version.zip -d libogg
[ -d libvorbis ] || unzip -o $packagedir/libvorbis-$vorbis_version.zip -d libvorbis
[ -d libcurl ] || unzip -o $packagedir/curl-$curl_version.zip -d libcurl
[ -d freetype ] || unzip -o $packagedir/freetype2-$freetype_version.zip -d freetype
[ -d sqlite3 ] || unzip -o $packagedir/sqlite3-$sqlite3_version.zip -d sqlite3
[ -d openal_stripped ] || unzip -o $packagedir/openal_stripped.zip
[ -d luajit ] || unzip -o $packagedir/luajit-$luajit_version.zip -d luajit

# Get minetest
cd $builddir
if [ ! "x$EXISTING_MINETEST_DIR" = "x" ]; then
	ln -s $EXISTING_MINETEST_DIR minetest
else
	[ -d minetest ] && (cd minetest && git pull) || (git clone https://github.com/kilbith/minetest)
fi
cd minetest
git_hash=$(git rev-parse --short HEAD)

# Get minetest_game
cd games
if [ "x$NO_MINETEST_GAME" = "x" ]; then
	[ -d minetest_game ] && (cd minetest_game && git pull) || (git clone --recursive https://github.com/kilbith/minetest_game)
fi
cd ../..

# Build the thing
cd minetest
[ -d _build ] && rm -Rf _build/
mkdir _build
cd _build
cmake .. \
	-DCMAKE_TOOLCHAIN_FILE=$toolchain_file \
	-DCMAKE_INSTALL_PREFIX=/tmp \
	-DVERSION_EXTRA=$git_hash \
	-DBUILD_CLIENT=1 -DBUILD_SERVER=0 \
	\
	-DENABLE_SOUND=1 \
	-DENABLE_CURL=1 \
	-DENABLE_FREETYPE=1 \
	\
	-DIRRLICHT_INCLUDE_DIR=$libdir/irrlicht/include \
	-DIRRLICHT_LIBRARY=$libdir/irrlicht/lib/Win64-gcc/libIrrlicht.dll.a \
	-DIRRLICHT_DLL=$libdir/irrlicht/bin/Win64-gcc/Irrlicht.dll \
	\
	-DZLIB_INCLUDE_DIR=$libdir/zlib/include \
	-DZLIB_LIBRARIES=$libdir/zlib/lib/libz.dll.a \
	-DZLIB_DLL=$libdir/zlib/bin/zlib1.dll \
	\
	-DLUA_INCLUDE_DIR=$libdir/luajit/include \
	-DLUA_LIBRARY=$libdir/luajit/libluajit.a \
	\
	-DOGG_INCLUDE_DIR=$libdir/libogg/include \
	-DOGG_LIBRARY=$libdir/libogg/lib/libogg.dll.a \
	-DOGG_DLL=$libdir/libogg/bin/libogg-0.dll \
	\
	-DVORBIS_INCLUDE_DIR=$libdir/libvorbis/include \
	-DVORBIS_LIBRARY=$libdir/libvorbis/lib/libvorbis.dll.a \
	-DVORBIS_DLL=$libdir/libvorbis/bin/libvorbis-0.dll \
	-DVORBISFILE_LIBRARY=$libdir/libvorbis/lib/libvorbisfile.dll.a \
	-DVORBISFILE_DLL=$libdir/libvorbis/bin/libvorbisfile-3.dll \
	\
	-DOPENAL_INCLUDE_DIR=$libdir/openal_stripped/include/AL \
	-DOPENAL_LIBRARY=$libdir/openal_stripped/lib/libOpenAL32.dll.a \
	-DOPENAL_DLL=$libdir/openal_stripped/bin/OpenAL32.dll \
	\
	-DCURL_DLL=$libdir/libcurl/bin/libcurl-4.dll \
	-DCURL_INCLUDE_DIR=$libdir/libcurl/include \
	-DCURL_LIBRARY=$libdir/libcurl/lib/libcurl.dll.a \
	\
	-DFREETYPE_INCLUDE_DIR_freetype2=$libdir/freetype/include/freetype2 \
	-DFREETYPE_INCLUDE_DIR_ft2build=$libdir/freetype/include/freetype2 \
	-DFREETYPE_LIBRARY=$libdir/freetype/lib/libfreetype.dll.a \
	-DFREETYPE_DLL=$libdir/freetype/bin/libfreetype-6.dll \
	\
	-DSQLITE3_INCLUDE_DIR=$libdir/sqlite3/include \
	-DSQLITE3_LIBRARY=$libdir/sqlite3/lib/libsqlite3.dll.a \
	-DSQLITE3_DLL=$libdir/sqlite3/bin/libsqlite3-0.dll \

make -j$(nproc)

[ "x$NO_PACKAGE" = "x" ] && make package

unzip *-win*.zip; rm *-win*.zip

cp /usr/x86_64-w64-mingw32/bin/libgcc*.dll ./kidscode-win64/*-win64/bin
cp /usr/x86_64-w64-mingw32/bin/libstdc++*.dll ./kidscode-win64/*-win64/bin
cp /usr/x86_64-w64-mingw32/bin/libwinpthread*.dll ./kidscode-win64/*-win64/bin

cd kidscode-win64
zip kidscode.zip -r *-win64

exit 0
# EOF
