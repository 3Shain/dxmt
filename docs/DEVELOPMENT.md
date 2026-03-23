# Build DXMT

[Our CI](/.github/workflows/ci.yml) is the best reference.

## Prerequisites:
- MacOS Sonoma and later. <sub>It doesn't make a lot of sense to build a project targeting Apple platforms from Windows, Linux or other OSs.</sub>
- Meson 1.3+, it's the build system used by this project
- LLVM 15 (exact major version), with headers and static libs
    - CMake 3.27+, when building LLVM from the source
- Xcode 16+, with Metal toolchain
- Wine 8+, with headers and tools, _if you want to use DXMT with Wine_.
    - A cross-compiler (mingw-w64 or llvm-mingw) for Windows is also required
    - When building DXMT with Wine, it is a **Cross Build**.

The following documentation is NOT for beginners. You are assumed to be familiar with C/C++ compilers and build systems.

## Setup LLVM

First of all, you need to build LLVM (or use a pre-built LLVM package) no matter if you want to use DXMT with Wine or as a native library. Then you need to decide which architecture should you target for LLVM: `x86_64` or `arm64`.

- For a **Cross Build**, you most likely want `x86_64` at the moment.
- Otherwise you probably should use `arm64`, since Intel Macs and Rosetta 2 are being deprecated.

This is an example of building `x86_64` LLVM
```sh
# in root directory of the project
mkdir -p ./toolchains/llvm-build
echo '**' >> toolchains/.gitignore
git clone --depth 1 --branch llvmorg-15.0.7 https://github.com/llvm/llvm-project.git toolchains/llvm-project

cmake -B ./toolchains/llvm-build -S ./toolchains/llvm-project/llvm \
  -DCMAKE_INSTALL_PREFIX="$(pwd)/toolchains/llvm" \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DLLVM_HOST_TRIPLE=x86_64-apple-darwin \
  -DLLVM_ENABLE_ASSERTIONS=On \
  -DLLVM_ENABLE_ZSTD=Off \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD="" \
  -DLLVM_BUILD_TOOLS=Off \
  -DBUG_REPORT_URL="https://github.com/3Shain/dxmt" \
  -DPACKAGE_VENDOR="DXMT" \
  -DLLVM_VERSION_PRINTER_SHOW_HOST_TARGET_INFO=Off \
  -G Ninja
cmake --build ./toolchains/llvm-build
cmake --install ./toolchains/llvm-build
```

Then `./toolchains/llvm` contains what we need later.

For `arm64` target, just replace any occurance of `x86_64` to `arm64` above.

## Setup Wine

> Skip this part if you are NOT doing a **Cross Build**

See [winehq.org - MacOS Building](https://gitlab.winehq.org/wine/wine/-/wikis/MacOS-Building)

You should also check the target architecture of Wine build, `--enable-archs=i386,x86_64` is a popular option when you also want x86 (32-bit programs) support in addition to x86_64.

If you build Wine from source, you don't have to install it, just pass the build directory later.

## Build DXMT

### Cross Build

Build (64-bit) Windows PE+ dlls **and** 64-bit unixlib (a Mach-O library with `.so` extension by convention).

```sh
meson setup --cross-file build-win64.txt -Dnative_llvm_path=</path/to/llvm> -Dwine_build_path=</path/to/wine/build> build --buildtype release
meson compile -C build
```

(Optional) Build (32-bit) Windows PE dlls

```sh
meson setup --cross-file build-win32.txt -Dwine_build_path=</path/to/wine/build> build32 --buildtype release
meson compile -C build32
```

> You can't build 32-bit dlls only, they need 64-bit unixlib to be functional

There are other compile options in [meson.options](/meson.options).

#### `clangd` configuration for proper language server support 

Since this project contains code runs on macOS/Windows(Wine)/both, `clangd` may not always be able to find the correct sysroot for e.g. libc++ include files. Add clangd argument `--query-driver=**/x86_64-w64-mingw32-**` may solve this problem. 

### Native Build

Build all components as Mach-O .dylib

```sh
meson setup -Dnative_llvm_path=</path/to/llvm> --buildtype release
meson compile -C build
```

#### Side notes on building x86_64 target from arm64 device/environment

Apparently the simplist solution is to use a x86_64 shell, but you can also set following environment variables

```sh
export CFLAGS='-arch x86_64'
export CXXFLAGS='-arch x86_64'
# then perform `meson setup ...`
```


# Debugging
The following environment variables can be used for **debugging** purposes.
- `MTL_SHADER_VALIDATION=1` Enable Metal shader validation layer
- `MTL_DEBUG_LAYER=1` Enable Metal API validation layer
- `MTL_CAPTURE_ENABLED=1` Enable Metal frame capture
- `DXMT_CAPTURE_EXECUTABLE="the executable name without extension"` Must be set to enable Metal frame capture. Press F10 to generate a capture. The captured result will be stored in the same directory as the executable.
- `DXMT_CAPTURE_FRAME=n` Automatically capture n-th frame. Useful for debugging a replay.
- `DXMT_LOG_LEVEL=none|error|warn|info|debug` Controls message logging.
- `DXMT_LOG_PATH=/some/directory` Changes path where log files are stored. Set to `none` to disable log file creation entirely, without disabling logging.
- `DXMT_SHADER_CACHE=0`: Disables the internal shader cache.
- `DXMT_SHADER_CACHE_PATH=/some/absolute/darwin/directory`: Path to internal shader cache files. Default to `$(getconf DARWIN_USER_CACHE_DIR)/dxmt/<executable name with extension>`.


### Logs
When used with Wine, DXMT will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `DXMT_LOG_PATH` variable, where log files in the given directory will be called `app_d3d11.log`, `app_dxgi.log` etc., where `app` is the name of the game executable.
