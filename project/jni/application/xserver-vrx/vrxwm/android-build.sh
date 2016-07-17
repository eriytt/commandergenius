#!/bin/sh

if [ ! -e configure ]; then
    libtoolize && aclocal && automake --add-missing && autoconf || exit 1
fi

echo "Building vrxwm for $1"

GVRSDK=$NDK/../gvr-android-sdk
GVRNDK=$GVRSDK/ndk-beta
ARCH=$1


case $ARCH in
    armeabi)
	TARGET_HOST=armv5te-none-linux-androideabi
	TARGET_ARCH=arch-arm
	;;
    armeabi-v7a)
	TARGET_HOST=arm-linux-androideabi
	TARGET_ARCH=arch-arm
	;;
    x86)
	TARGET_HOST=i686-linux-android
	TARGET_ARCH=arch-x86
	;;
    mips)
	TARGET_HOST=mipsel-linux-android
	TARGET_ARCH=arch-mips
	;;
esac

builddir=build-$ARCH

mkdir -p $builddir
cd $builddir

#echo NDK_TOOLCHAIN_VERSION=$NDK_TOOLCHAIN_VERSION
export

[ -e Makefile ] || \
    LDFLAGS=--sysroot=$NDK/platforms/$PLATFORMVER/$TARGET_ARCH \
	   CXXFLAGS="--sysroot=$NDK/platforms/$PLATFORMVER/$TARGET_ARCH -isystem$NDK/sources/cxx-stl/gnu-libstdc++/$GCCVER/include -isystem$NDK/sources/cxx-stl/gnu-libstdc++/$GCCVER/libs/$ARCH/include -I$GVRNDK/include -D__ANDROID__ -std=c++11" \
	   ../configure --build i686-pc-linux-gnu --host $TARGET_HOST || exit 1

make || exit 1
