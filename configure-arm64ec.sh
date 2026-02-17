# TODO: move this into configure.sh (refactoring)

mkdir -p ./toolchains


# install wine
curl -SL https://github.com/3Shain/wine/releases/download/wine-11.2/wine-11.2.tar.gz > ./wine.tar.gz
tar -zvxf ./wine.tar.gz -C ./toolchains
rm ./wine.tar.gz
# install mingw-llvm
curl -SL https://github.com/mstorsjo/llvm-mingw/releases/download/20251216/llvm-mingw-20251216-ucrt-macos-universal.tar.xz > ./llvm.tar.xz
tar -zvxf ./llvm.tar.xz -C ./toolchains
rm ./llvm.tar.xz

git clone --depth 1 --branch llvmorg-15.0.7 https://github.com/llvm/llvm-project.git toolchains/llvm-project 

mkdir -p ./toolchains/llvm-darwin-build-arm64
cmake -B ./toolchains/llvm-darwin-build-arm64 -S ./toolchains/llvm-project/llvm \
  -DCMAKE_INSTALL_PREFIX="$(pwd)/toolchains/llvm-darwin" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DLLVM_HOST_TRIPLE=arm64-apple-darwin \
  -DLLVM_ENABLE_ASSERTIONS=On \
  -DLLVM_ENABLE_ZSTD=Off \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_BUILD_TOOLS=Off \
  -DBUG_REPORT_URL="https://github.com/3Shain/dxmt" \
  -DPACKAGE_VENDOR="DXMT" \
  -DLLVM_VERSION_PRINTER_SHOW_HOST_TARGET_INFO=Off \
  -G Ninja
pushd ./toolchains/llvm-darwin-build-arm64
ninja
ninja install
popd
