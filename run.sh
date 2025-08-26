#docker run --rm \
#  -v $(pwd):/src \
#  -u $(id -u):$(id -g) \
#  emscripten/emsdk \
#  bash -c 'emcc perl5/perl.js -o perl.html'

docker run --rm \
  -v $(pwd):/src \
  -u $(id -u):$(id -g) \
  emscripten/emsdk \
#  bash -c '/emsdk/upstream/bin/llvm-objdump -s test.o'
  bash -c 'emcc test.wasm -o page.html'
