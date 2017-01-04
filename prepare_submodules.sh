#!/bin/bash

# Get submodules

git submodule update --init --recursive
pushd project/jni/application/xserver-vrx/
git clone -b master git@github.com:eriytt/xserver-xsdl.git xserver
cd ..
ln -s xserver-vrx src
popd

