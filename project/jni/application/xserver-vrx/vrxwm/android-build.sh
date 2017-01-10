#!/bin/sh

if [ ! -e configure ]; then
    touch NEWS README AUTHORS ChangeLog
    libtoolize && aclocal && autoheader && automake --add-missing && autoconf || exit 1
fi

echo "Building vrxwm for $1"

GVRSDK=$NDK/../gvr-android-sdk
GVRNDK=$GVRSDK/ndk
ARCH=$1


case $ARCH in
    armeabi)
	TARGET_HOST=armv5te-none-linux-androideabi
	TARGET_ARCH=arch-arm
	NDK_ARCH=android_arm # ?
	;;
    armeabi-v7a)
	TARGET_HOST=arm-linux-androideabi
	TARGET_ARCH=arch-arm
	NDK_ARCH=android_arm
	;;
    x86)
	TARGET_HOST=i686-linux-android
	TARGET_ARCH=arch-x86
	NDK_ARCH=android_x86
	;;
    mips)
	TARGET_HOST=mipsel-linux-android
	TARGET_ARCH=arch-mips
	echo "MIPS architecture not supported"
	exit 1
	;;
esac

builddir=build-$ARCH

mkdir -p $builddir
cd $builddir

LDFLAGS="--sysroot=$NDK/platforms/$PLATFORMVER/$TARGET_ARCH -L$NDK/sources/cxx-stl/gnu-libstdc++/$GCCVER/libs/$ARCH -L$GVRNDK/lib/$NDK_ARCH -lgnustl_shared -llog -lEGL -lGLESv2 -lgvr"

CXXFLAGS="--sysroot=$NDK/platforms/$PLATFORMVER/$TARGET_ARCH -isystem$NDK/sources/cxx-stl/gnu-libstdc++/$GCCVER/include -isystem$NDK/sources/cxx-stl/gnu-libstdc++/$GCCVER/libs/$ARCH/include -I$GVRNDK/include -I../../xserver/android/$ARCH -D__ANDROID__ -std=c++11 -Wall -I../../xserver/hw/kdrive/vrx"

[ -e Makefile ] || \
    LDFLAGS="$LDFLAGS" \
	   CXXFLAGS="$CXXFLAGS" \
	   ../configure --build i686-pc-linux-gnu --host $TARGET_HOST || exit 1

make || exit 1
