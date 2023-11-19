export ROOT=$(pwd)

cp $ROOT/build/src/winemetal/unix/winemetal.so $ROOT/toolchains/wine/lib/wine/x86_64-unix/winemetal.so
cp $ROOT/build/src/winemetal/winemetal.dll $ROOT/toolchains/wine/lib/wine/x86_64-windows/winemetal.dll
cp $ROOT/build/src/d3d11/d3d11.dll $ROOT/wineprefix/drive_c/windows/system32/d3d11.dll
cp $ROOT/build/src/dxgi/dxgi.dll $ROOT/wineprefix/drive_c/windows/system32/dxgi.dll

export WINEDEBUG=fixme-all

cd $ROOT/build/tests/airconv

MTL_HUD_ENABLED=1 OBJC_DEBUG_MISSING_POOLS=NO MTL_CAPTURE_ENABLED=0 WINEPREFIX=$ROOT/wineprefix XDG_DATA_HOME=$ROOT/.local/share XDG_CONFIG_HOME=$ROOT/.config \
XDG_CACHE_HOME=$ROOT/.cache WINEDEBUG="" WINEDLLOVERRIDES="dxgi,d3d11,d3dcompiler_47=n,b" $ROOT/toolchains/wine/bin/wine generate_kernel.exe
