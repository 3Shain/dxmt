mkdir -p ./toolchains
# install wine
curl -SL https://github.com/3Shain/wine/releases/download/v8.16-3shain/wine.tar.gz > ./wine.tar.gz
mkdir -p ./toolchains/wine
tar -zvxf ./wine.tar.gz -C ./toolchains/wine
rm ./wine.tar.gz
# install mingw-llvm
curl -SL https://github.com/mstorsjo/llvm-mingw/releases/download/20231017/llvm-mingw-20231017-ucrt-macos-universal.tar.xz > ./llvm.tar.xz

tar -zvxf ./llvm.tar.xz -C ./toolchains
rm ./llvm.tar.xz


# setup llvm
git clone --depth 1 https://github.com/llvm/llvm-project.git toolchains/llvm-project 
mkdir -p ./toolchains/llvm-build
cmake -B ./toolchains/llvm-build -S ./toolchains/llvm-project/llvm -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_INSTALL_PREFIX="$(pwd)/toolchains/llvm" \
  -DLLVM_HOST_TRIPLE=x86_64-w64-mingw32 -DLLVM_TARGETS_TO_BUILD=X86 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_SYSROOT="$(pwd)/toolchains/llvm-mingw-20231017-ucrt-macos-universal" \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -G Ninja
pushd ./toolchains/llvm-build
ninja
ninja install
popd

## configure
# export PATH="$PATH:$(pwd)/toolchains/llvm-mingw-20231017-ucrt-macos-universal/bin"
# meson setup --cross-file build-win64.txt --native-file build-osx.txt build
## build
# meson compile -C build