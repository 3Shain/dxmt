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

## configure
# export PATH="$PATH:$(pwd)/toolchains/llvm-mingw-20231017-ucrt-macos-universal/bin"
# meson setup --cross-file build-win64.txt --native-file build-osx.txt build
ß
## build
# meson compile -C build
