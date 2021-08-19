#!/bin/bash

VERSION=`head debian/changelog  -n 1 | cut -d \( -f 2 | cut -d \) -f 1 | cut -d \- -f 1`
RELEASE=`head debian/changelog  -n 1 | cut -d \( -f 2 | cut -d \) -f 1 | cut -d \- -f 2`
PKG_NAME=log2mail
PKG_NAME_UPSTREAM=log2mail

echo "VERSION: $VERSION"
echo "RELEASE: $RELEASE"
( cd ..; tar czf ${PKG_NAME}_${VERSION}.orig.tar.gz --exclude-vcs --exclude=debian --exclude=.pc ${PKG_NAME_UPSTREAM})

dpkg-source -b .
