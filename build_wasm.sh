#!/bin/bash

#rm -rf perl5
#git submodule update --init
#git apply perl5.patch

#cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
#  -DCMAKE_TOOLCHAIN_FILE=wasi-sdk.cmake \
#  -DWASI_SDK_PREFIX="/Users/alexanderzhang/Documents/wasi-sdk-20.0"

#rm -rf hostbuild
#docker run --rm -v $(pwd):/src emscripten/emsdk \
#    bash -c 'cmake -S . -B hostbuild -DTOOLS_ONLY=ON '`
#        `'-DCMAKE_BUILD_TYPE=Release && '`
#        `'cmake --build hostbuild --config Release'

rm -rf build
docker run --rm -v $(pwd):/src emscripten/emsdk \
    bash -c 'emcmake cmake -S . -B build '`
        `'-DCMAKE_BUILD_TYPE=Release && '`
        `'cmake --build build --config Release'
