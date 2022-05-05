#!/bin/bash

echo "Start of building";
if [ ! -d "./cmake-build-release" ]; then
    conan instal . --instal-folder cmake-build-release --build=missing
    cmake . -DCMAKE_TOOLCHAIN_FILE=cmake-build-release/conan_toolchain.cmake
fi
cmake --build .
echo "The build is finished, now you can use time_server";