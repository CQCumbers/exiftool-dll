# exiftool-dll
> Standalone exiftool DLL for C

This project builds exiftool as a single, self-contained DLL with C interface. It includes its own perl interpreter and lightweight wrapper over perl types, hopefully making it easier to integrate and interact with compared to e.g. shipping a seperate exiftool exe and managing it as a seperate process.

## Building
The latest builds for Windows and macOS can be downloaded from [nightly.link](https://nightly.link/CQCumbers/exiftool-dll/workflows/build/master). To build from source, run the following:

```
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix install
```

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=wasi-sdk.cmake \
  -DWASI_SDK_PREFIX="wasi-sdk-path"
```
